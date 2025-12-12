#include <Chrono.h>
#include <SystemInfo.h>
#include <Scheduler.h>
#include <Timer.h>
#include <gtest/gtest.h>
#include <atomic>
#include "../Source/win32.h"

namespace TestTimer {

class TestTimer :
	public ::testing::Test
{
protected:
	TestTimer ()
	{}

	virtual ~TestTimer ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		Nirvana::Core::SystemInfo::initialize ();
		Nirvana::Core::Heap::initialize ();
		Nirvana::Core::Chrono::initialize ();
		Nirvana::Core::Timer::initialize ();
		Nirvana::Core::Scheduler::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		if (Nirvana::Core::Timer::initialized ())
			Nirvana::Core::Timer::terminate ();
		Nirvana::Core::Port::Chrono::terminate ();
		Nirvana::Core::Scheduler::terminate ();
		Nirvana::Core::Heap::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
	}
};

class TimerTest :
	public Nirvana::Core::Port::Timer
{
public:
	TimerTest () :
		signalled_ (0)
	{}

	~TimerTest ()
	{}

protected:
	virtual void signal () noexcept
	{
		++signalled_;
	}

public:
	std::atomic <unsigned> signalled_;
};

TEST_F (TestTimer, Set)
{
	TimerTest timer;
	timer.set (0, 1 * TimeBase::SECOND, 0);
	Sleep (1500);
	EXPECT_EQ (timer.signalled_, 1);
}

TEST_F (TestTimer, Cancel)
{
	TimerTest timer;
	timer.set (0, 1 * TimeBase::SECOND, 0);
	timer.cancel ();
	Sleep (1500);
	EXPECT_EQ (timer.signalled_, 0);
}

TEST_F (TestTimer, Destruct)
{
	{
		TimerTest timer;
		timer.set (0, 1 * TimeBase::SECOND, 0);
	}
	Sleep (2000);
}

TEST_F (TestTimer, Shutdown)
{
	TimerTest timer;
	timer.set (0, 1 * TimeBase::SECOND, 0);
	Nirvana::Core::Timer::terminate ();
	Sleep (2000);
	EXPECT_EQ (timer.signalled_, 0);
}

TEST_F (TestTimer, Periodic)
{
	TimerTest timer;
	timer.set (0, 1 * TimeBase::SECOND, 1 * TimeBase::SECOND);
	Sleep (500);
	EXPECT_EQ (timer.signalled_, 0);
	Sleep (1000);
	EXPECT_EQ (timer.signalled_, 1);
	Sleep (1000);
	EXPECT_EQ (timer.signalled_, 2);
}

TEST_F (TestTimer, Absolute)
{
	TimerTest timer;
	TimeBase::TimeT t = Nirvana::Core::Port::Chrono::UTC ().time () + 1 * TimeBase::SECOND;
	timer.set (TimerTest::TIMER_ABSOLUTE, t, 0);
	Sleep (500);
	EXPECT_EQ (timer.signalled_, 0);
	Sleep (1000);
	EXPECT_EQ (timer.signalled_, 1);
}

}
