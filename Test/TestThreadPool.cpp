#include "../Source/PostOffice.h"
#include "../Source/Mailslot.h"
#include "../Source/MailslotName.h"
#include "../Source/ThreadPostman.h"
#include "../Port/SystemInfo.h"
#include <gtest/gtest.h>
#include <atomic>

namespace TestThreadPool {

using namespace ::std;
using namespace ::Nirvana::Core::Windows;

class TestThreadPool :
	public ::testing::Test
{
protected:
	TestThreadPool ()
	{}

	virtual ~TestThreadPool ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		Nirvana::Core::Port::SystemInfo::initialize ();
		Nirvana::Core::Heap::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Nirvana::Core::Heap::terminate ();
	}
};

struct Message
{
	LONG_PTR tag;
};

const unsigned TAG = 123456789;

class PostOffice :
	public ::Nirvana::Core::Windows::PostOffice <PostOffice, sizeof (Message), THREAD_PRIORITY_NORMAL>
{
	typedef ::Nirvana::Core::Windows::PostOffice <PostOffice, sizeof (Message), THREAD_PRIORITY_NORMAL> Base;
public:
	PostOffice ()
	{
		Base::create_mailslot (MailslotName (GetCurrentProcessId ()));
		Base::start ();
	}

	~PostOffice ()
	{
		Base::terminate ();
	}

	void received (void* message, DWORD size)
	{
		assert (size == sizeof (Message));
		assert (((Message*)message)->tag == TAG);
		++message_cnt_;
	}

	size_t message_cnt () const
	{
		return message_cnt_;
	}

private:
	atomic <size_t> message_cnt_;
};

TEST_F (TestThreadPool, PostOffice)
{
	Mailslot mailslot;
	EXPECT_FALSE (mailslot.open (MailslotName (GetCurrentProcessId ())));
	PostOffice po;
	EXPECT_TRUE (mailslot.open (MailslotName (GetCurrentProcessId ())));

	static const unsigned MESSAGE_CNT = 100;
	for (unsigned i = 0; i < MESSAGE_CNT; ++i) {
		Message msg;
		msg.tag = TAG;
		mailslot.send (msg);
	}
	
	Sleep (3000);
	EXPECT_EQ (po.message_cnt (), MESSAGE_CNT);
}

class IO_Req : public OVERLAPPED
{
public:
	IO_Req ()
	{
		memset (static_cast <OVERLAPPED*> (this), 0, sizeof (OVERLAPPED));
	}
};

class IO_WaitList : public IO_Req
{
public:
	IO_WaitList () :
		event_ (CreateEventW (nullptr, true, false, nullptr)),
		error_ (0)
	{}

	int wait ()
	{
		WaitForSingleObject (event_, INFINITE);
		return error_;
	}

	void release (int error)
	{
		error_ = error;
		SetEvent (event_);
	}

private:
	HANDLE event_;
	int error_;
};

class File :
	public CompletionPortReceiver
{
public:
	File () :
		handle_ (INVALID_HANDLE_VALUE)
	{}

	~File ()
	{
		close ();
	}

	int open (PostOffice& po, const WCHAR* name)
	{
		close ();
		handle_ = CreateFileW (name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, nullptr);
		if (INVALID_HANDLE_VALUE == handle_)
			return GetLastError ();
		po.add_receiver (handle_, *this);
		return 0;
	}

	void close ()
	{
		if (handle_ != INVALID_HANDLE_VALUE) {
			CloseHandle (handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
	}

	int start_read (uint64_t pos, uint32_t size, void* buf, IO_WaitList& wl)
	{
		wl.Offset = (DWORD)pos;
		wl.OffsetHigh = (DWORD)(pos >> 32);
		if (!ReadFile (handle_, buf, size, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
		return 0;
	}

	int start_write (uint64_t pos, uint32_t size, const void* buf, IO_WaitList& wl)
	{
		wl.Offset = (DWORD)pos;
		wl.OffsetHigh = (DWORD)(pos >> 32);
		if (!WriteFile (handle_, buf, size, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
		return 0;
	}

private:
	virtual void completed (_OVERLAPPED* ovl, uint32_t size, uint32_t error) NIRVANA_NOEXCEPT
	{
		static_cast <IO_WaitList*> (ovl)->release (error);
	}

private:
	HANDLE handle_;
};

template <typename El>
class VMArray
{
	VMArray (const VMArray&) = delete;
public:
	VMArray (size_t cnt)
	{
		buf_ = VirtualAlloc (nullptr, cnt * sizeof (El), MEM_COMMIT, PAGE_READWRITE);
		if (!buf_)
			throw std::bad_alloc ();
	}

	~VMArray ()
	{
		VirtualFree (buf_, 0, MEM_RELEASE);
	}

	operator El* ()
	{
		return (El*)buf_;
	}

private:
	void* buf_;
};

TEST_F (TestThreadPool, File)
{
	WCHAR path [MAX_PATH + 1];
	GetModuleFileNameW (nullptr, path, (DWORD)size (path));
	wcscpy (wcsrchr (path, L'\\') + 1, L"TestThreadPool.File.tmp");

	{
		PostOffice po;
		File f;
		ASSERT_FALSE (f.open (po, path));

		static const size_t BLOCK_SIZE = 0x10000;
		static const size_t BLOCK_CNT = 16;

		VMArray <size_t> buf (BLOCK_SIZE / sizeof (size_t));
		{
			// Test for write beyond the end
			IO_WaitList wl;
			EXPECT_FALSE (f.start_write (BLOCK_SIZE, BLOCK_SIZE, buf, wl));
			EXPECT_FALSE (wl.wait ());
		}

		for (size_t ib = 0; ib < BLOCK_CNT; ++ib) {
			size_t tag = ib * BLOCK_SIZE / sizeof (size_t);
			for (size_t* p = buf; p != buf + BLOCK_SIZE / sizeof (size_t); ++p) {
				*p = tag++;
			}
			IO_WaitList wl;
			ASSERT_FALSE (f.start_write ((uint64_t)ib * (uint64_t)BLOCK_SIZE, BLOCK_SIZE, buf, wl));
			ASSERT_FALSE (wl.wait ());
		}

		for (size_t i = 0; i < BLOCK_CNT * 100; ++i) {
			size_t ib = BLOCK_CNT * rand () / (RAND_MAX + 1);
			IO_WaitList wl;
			ASSERT_FALSE (f.start_read ((uint64_t)ib * (uint64_t)BLOCK_SIZE, BLOCK_SIZE, buf, wl));
			ASSERT_FALSE (wl.wait ());
			size_t tag = ib * BLOCK_SIZE / sizeof (size_t);
			for (size_t* p = buf; p != buf + BLOCK_SIZE / sizeof (size_t); ++p) {
				EXPECT_EQ (*p, tag++);
			}
		}

		IO_WaitList wl;
		EXPECT_FALSE (f.start_read ((uint64_t)BLOCK_CNT * (uint64_t)BLOCK_SIZE, BLOCK_SIZE, buf, wl));
		EXPECT_TRUE (wl.wait ());
	}

	DeleteFileW (path);
}

}
