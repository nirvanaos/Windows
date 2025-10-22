#include "../Source/PostOffice.h"
#include "../Source/Mailslot.h"
#include "../Source/ObjectName.h"
#include "../Source/ThreadPostman.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>
#include <atomic>

namespace TestThreadPool {

using namespace ::Nirvana::Core::Windows;
using namespace ::Nirvana::Core::Port;

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
		Nirvana::Core::SystemInfo::initialize ();
		ASSERT_TRUE (Nirvana::Core::Heap::initialize ());
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Nirvana::Core::Heap::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
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
		Base::create_mailslot (ObjectName (MAILSLOT_PREFIX, GetCurrentProcessId ()));
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
	std::atomic <size_t> message_cnt_;
};

TEST_F (TestThreadPool, PostOffice)
{
	Mailslot mailslot;
	EXPECT_FALSE (mailslot.open (ObjectName (MAILSLOT_PREFIX, GetCurrentProcessId ())));
	PostOffice po;
	EXPECT_TRUE (mailslot.open (ObjectName (MAILSLOT_PREFIX, GetCurrentProcessId ())));

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

//#define USE_SCATTER_GATHER

class File :
	public CompletionPortReceiver
{
public:
	static const size_t BLOCK_SIZE = 0x10000;
	static const size_t PAGE_SIZE = 0x1000;

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

	int start_read (uint64_t pos, void* buf, IO_WaitList& wl)
	{
		wl.Offset = (DWORD)pos;
		wl.OffsetHigh = (DWORD)(pos >> 32);
#ifdef USE_SCATTER_GATHER
		FILE_SEGMENT_ELEMENT* pseg = segment_array_;
		for (uint8_t* p = (uint8_t*)buf, *end = p + BLOCK_SIZE; p < end; p += PAGE_SIZE, ++pseg) {
			pseg->Buffer = PtrToPtr64 (p);
		}
		pseg->Buffer = 0;
		if (!ReadFileScatter (handle_, segment_array_, BLOCK_SIZE, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
#else
		if (!ReadFile (handle_, buf, BLOCK_SIZE, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
#endif
		return 0;
	}

	int start_write (uint64_t pos, const void* buf, IO_WaitList& wl)
	{
		wl.Offset = (DWORD)pos;
		wl.OffsetHigh = (DWORD)(pos >> 32);
#ifdef USE_SCATTER_GATHER
		FILE_SEGMENT_ELEMENT* pseg = segment_array_;
		for (uint8_t* p = (uint8_t*)buf, *end = p + BLOCK_SIZE; p < end; p += PAGE_SIZE, ++pseg) {
			pseg->Buffer = PtrToPtr64 (p);
		}
		pseg->Buffer = 0;
		if (!WriteFileGather (handle_, segment_array_, BLOCK_SIZE, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
#else
		if (!WriteFile (handle_, buf, BLOCK_SIZE, nullptr, &static_cast <OVERLAPPED&> (wl))) {
			DWORD err = GetLastError ();
			if (ERROR_IO_PENDING != err)
				return err;
		}
#endif
		return 0;
	}

private:
	virtual void completed (_OVERLAPPED* ovl, uint32_t size, uint32_t error) noexcept
	{
		static_cast <IO_WaitList*> (ovl)->release (error);
	}

private:
	HANDLE handle_;
#ifdef USE_SCATTER_GATHER
	FILE_SEGMENT_ELEMENT segment_array_ [BLOCK_SIZE / PAGE_SIZE + 1];
#endif
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
	GetModuleFileNameW (nullptr, path, (DWORD)std::size (path));
	wcscpy (wcsrchr (path, L'\\') + 1, L"TestThreadPool.File.tmp");

	{
		PostOffice po;
		File f;
		ASSERT_FALSE (f.open (po, path));

		static const size_t BLOCK_CNT = 16;

		VMArray <size_t> buf (File::BLOCK_SIZE / sizeof (size_t));
		{
			// Test for write beyond the end
			IO_WaitList wl;
			EXPECT_FALSE (f.start_write (File::BLOCK_SIZE, buf, wl));
			EXPECT_FALSE (wl.wait ());
		}

		for (size_t ib = 0; ib < BLOCK_CNT; ++ib) {
			size_t tag = ib * File::BLOCK_SIZE / sizeof (size_t);
			for (size_t* p = buf; p != buf + File::BLOCK_SIZE / sizeof (size_t); ++p) {
				*p = tag++;
			}
			IO_WaitList wl;
			ASSERT_FALSE (f.start_write ((uint64_t)ib * (uint64_t)File::BLOCK_SIZE, buf, wl));
			ASSERT_FALSE (wl.wait ());
		}

		for (size_t i = 0; i < BLOCK_CNT * 100; ++i) {
			size_t ib = BLOCK_CNT * rand () / (RAND_MAX + 1);
			IO_WaitList wl;
			ASSERT_EQ (0, f.start_read ((uint64_t)ib * (uint64_t)File::BLOCK_SIZE, buf, wl));
			ASSERT_FALSE (wl.wait ());
			size_t tag = ib * File::BLOCK_SIZE / sizeof (size_t);
			for (size_t* p = buf; p != buf + File::BLOCK_SIZE / sizeof (size_t); ++p) {
				EXPECT_EQ (*p, tag++);
			}
		}

		{
			// Test for read beyond the end
			IO_WaitList wl;
			int err = f.start_read ((uint64_t)BLOCK_CNT * (uint64_t)File::BLOCK_SIZE, buf, wl);
			ASSERT_TRUE (!err || ERROR_HANDLE_EOF == err);
			if (!err)
				err = wl.wait ();
			ASSERT_EQ (ERROR_HANDLE_EOF, err);
		}
	}

	DeleteFileW (path);
}

}
