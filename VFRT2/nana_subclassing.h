#pragma once
#include <map>
#include <functional>
#include <mutex>

#include <nana/gui.hpp>

#ifdef NANA_WINDOWS
#include <Windows.h>
#include <Winuser.h>
#endif


class subclass
{
	struct msg_pro
	{
		std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> before;
		std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> after;
	};

	typedef std::lock_guard<std::recursive_mutex> lock_guard;
public:
	subclass(nana::window wd)
		: native_(reinterpret_cast<HWND>(nana::API::root(wd))),
		old_proc_(nullptr)
	{
	}

	~subclass()
	{
		clear();
	}

	void make_before(UINT msg, std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> fn)
	{
		lock_guard lock(mutex_);
		msg_table_[msg].before = std::move(fn);
		_m_subclass(true);
	}

	void make_after(UINT msg, std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> fn)
	{
		lock_guard lock(mutex_);
		msg_table_[msg].after = std::move(fn);
		_m_subclass(true);
	}

	void umake_before(UINT msg)
	{
		lock_guard lock(mutex_);
		auto i = msg_table_.find(msg);
		if(msg_table_.end() != i)
		{
			i->second.before = nullptr;
			if(nullptr == i->second.after)
			{
				msg_table_.erase(msg);
				if(msg_table_.empty())
					_m_subclass(false);
			}
		}
	}

	void umake_after(UINT msg)
	{
		lock_guard lock(mutex_);
		auto i = msg_table_.find(msg);
		if(msg_table_.end() != i)
		{
			i->second.after = nullptr;
			if(nullptr == i->second.before)
			{
				msg_table_.erase(msg);
				if(msg_table_.empty())
					_m_subclass(false);
			}
		}
	}

	void umake(UINT msg)
	{
		lock_guard lock(mutex_);
		msg_table_.erase(msg);

		if(msg_table_.empty())
			_m_subclass(false);
	}

	void clear()
	{
		lock_guard lock(mutex_);
		msg_table_.clear();
		_m_subclass(false);
	}
private:
	void _m_subclass(bool enable)
	{
		lock_guard lock(mutex_);

		if(enable)
		{
			if(native_ && (nullptr == old_proc_))
			{
				old_proc_ = (WNDPROC)::SetWindowLongPtr(native_, -4 /* GWL_WNDPROC*/, (LONG_PTR)_m_subclass_proc);
				if(old_proc_)
					table_[native_] = this;
			}
		}
		else
		{
			if(old_proc_)
			{
				table_.erase(native_);
				::SetWindowLongPtr(native_, -4 /* GWL_WNDPROC*/, (LONG_PTR)old_proc_);
				old_proc_ = nullptr;

			}
		}
	}

	static bool _m_call_before(msg_pro& pro, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res)
	{
		return (pro.before ? pro.before(msg, wp, lp, res) : true);
	}

	static bool _m_call_after(msg_pro& pro, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res)
	{
		return (pro.after ? pro.after(msg, wp, lp, res) : true);
	}
private:
	static LRESULT CALLBACK _m_subclass_proc(HWND wd, UINT msg, WPARAM wp, LPARAM lp)
	{
		lock_guard lock(mutex_);

		subclass * self = _m_find(wd);
		if(nullptr == self || nullptr == self->old_proc_)
			return 0;

		auto i = self->msg_table_.find(msg);
		if(self->msg_table_.end() == i)
			return ::CallWindowProc(self->old_proc_, wd, msg, wp, lp);

		LRESULT res = 0;
		if(self->_m_call_before(i->second, msg, wp, lp, &res))
		{
			res = ::CallWindowProc(self->old_proc_, wd, msg, wp, lp);
			self->_m_call_after(i->second, msg, wp, lp, &res);
		}

		if(WM_DESTROY == msg)
			self->clear();

		return res;
	}

	static subclass * _m_find(HWND wd)
	{
		lock_guard lock(mutex_);
		std::map<HWND, subclass*>::iterator i = table_.find(wd);
		if(i != table_.end())
			return i->second;

		return 0;
	}
private:
	HWND native_;
	WNDPROC old_proc_;
	std::map<UINT, msg_pro> msg_table_;

	static std::recursive_mutex mutex_;
	static std::map<HWND, subclass*> table_;
};

std::recursive_mutex subclass::mutex_;
std::map<HWND, subclass*> subclass::table_;