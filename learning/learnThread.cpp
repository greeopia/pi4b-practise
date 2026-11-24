
namespace std {

class thread{
public:
    using native_handle_type = __gthread_t;

    class id{
        native_handle_type	_M_thread; // 线程句柄
        /*

            通过 native_handle() 拿到原生的 pthread_t 句柄后，这里可以像FreeRTOS一样自定义很多东西，通过posix接口
            想要超级自定义可以直接用pthread_create

            调度与调试
            pthread_setschedparam()
            pthread_attr_setschedpolicy()
            pthread_setschedprio()
            pthread_setname_np()
            sched_setscheduler()
            sched_setattr // through syscall() -> 孩子们真真可以手动系统调用，虽然是在thread开始之前
            不限于 main 函数——任何线程调用 pthread_create 创建新线程时，都会发生线程继承
                pthread_attr_setinheritsched() -> PTHREAD_INHERIT_SCHED PTHREAD_EXPLICIT_SCHED -> pthread_attr_t
                "在创建线程前，通过 pthread_attr_setinheritsched() 将 inheritsched 属性设为 PTHREAD_EXPLICIT_SCHED，强制新线程使用属性对象中显式设置的策略和优先级，而不是从父线程继承"

            线程在多linux核的绑定
            pthread_setaffinity_np() // cpu_set_t Cgroup
            pthread_getaffinity_np()

            内存相关：(mainly to avoid Page Fault)
            1.越过mmap: 
                mlockall(MCL_CURRENT | MCL_FUTURE); 
                pthread_attr_setstacksize()
                pthread_attr_setstack()

            signal && timers
            pthread_sigmask() // eee没了很多中断只有特定的几种signal...
            sigwait()
            sigwaitinfo()

        ---others---

            关于锁
            其实有很多() glibc好像有坑
            std::mutex pthread_mutex_t (其实现在mutex含有一部分spinlock) -> pthread_mutexattr_t
                pthread_mutexattr_settype() -> pthread_mutexattr_t
                    PTHREAD_MUTEX_NORMAL PTHREAD_MUTEX_ERRORCHECK PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_DEFAULT
                可以有pthread_mutexattr_setprotocol() -> PTHREAD_PRIO_INHERIT PTHREAD_PRIO_PROTECT
                pthread_mutexattr_setprioceiling()
            spinlock pthread_spinlock_t
            读写锁 std::shared_mutex pthread_rwlock_rdlock() pthread_rwlock_wdlock()

            条件变量 && 信号量 (可以与FreeRTOS衔接一下)
            ...
            消息队列，mailbox，软件定时器 ...etc

        */
    public:
        id() noexcept : _M_thread{} {}
        
        explicit id(native_handle_type __id){
            _M_thread = __id;
        }
    private:
        friend class thread;
        friend struct hash<id>;

        friend bool operator==(id __x, id __y) noexcept;
        friend bool operator<(id __x, id __y) noexcept;

        template<class _CharT, class _Traits>
        friend basic_ostream<_CharT, _Traits>& operator<<(basic_ostream<_CharT, _Traits>& __out, id __id);
    };

private:
    id _M_id; // thread 对象用 _M_id 记录它关联的线程

    template<typename _Tp>
    using __not_same = __not_<is_same<__remove_cvref_t<_Tp>, thread>>;

public:
    thread() noexcept = default;
    static void _M_thread_deps_never_run();

    template<typename _Callable, typename... _Args, typename = _Require<__not_same<_Callable>>>
    explicit thread(_Callable&& __f, _Args&&... __args) {
        static_assert(__is_invocable<typename decay<_Callable>::type, typename decay<_Args>::type...>::value, 
            "std::thread arguments must be invocable after conversion to rvalues");
        
        // 假设 work 的类型是 void(int)，包装器就持有 tuple<void (*)(int), int>。
        // 把包装器装进 _State_impl，用 unique_ptr 传给启动线程的入口。
        // using _State_ptr = unique_ptr<_State>;
        using _Wrapper = _Call_wrapper<_Callable, _Args...>;
        _M_start_thread(_State_ptr(new _State_impl<_Wrapper>(std::forward<_Callable>(__f), std::forward<_Args>(__args)...)), _M_thread_deps_never_run);
    }

    ~thread() {
        if(joinable()) std::__terminate(); // 那直接终止()
        /*
            一般来说
            if(th.joinable())
                th.join();

            emm...
        */
    }

    thread(const thread&) = delete;
    thread(thread&& __t) noexcept { swap(__t); }

    thread& operator=(const thread&) = delete;
    thread& operator=(thread&& __t) noexcept {
        if(joinable()) std::__terminate();
        swap(__t);
        return *this;
    }

    void swap(thread& __t) noexcept { std::swap(_M_id, __t._M_id); }
    bool joinable() const noexcept { return !(_M_id == id()); }
    void join();
    void detach();

    id get_id() const noexcept { return _M_id; }

    native_handle_type native_handle() { return _M_id._M_thread; } // 暴露句柄的api

    static unsigned int hardware_concurrency() noexcept;

private:
    struct _State {
        virtual ~_State();
        virtual void _M_run() = 0;
    };
    using _State_ptr = unique_ptr<_State>; // 真正保存thread对象的状态

    template<typename _Callable>
    struct _State_impl : public _State {
        _Callable _M_func;

        template<typename... _Args>
        _State_impl(_Args&&... __args) : _M_func(std::forward<_Args>(__args)...) {} // 看不懂喵

        void _M_run() { _M_func(); }
    };
    void _M_start_thread(_State_ptr, void (*)());

    template<typename _Tuple> // 对象参数合法化
    struct _Invoker {
        template<typename... _Args>
        explicit _Invoker(_Args&&... __args) : _M_t(std::forward<_Args>(__args)...) {}

        _Tuple _M_t;

        template<typename> struct __result;

        template<typename _Fn, typename... _Args>
        struct __result<tuple<_Fn, _Args...>> : __invoke_result<_Fn, _Args...> {};

        template<size_t... _Ind>
        typename __result<_Tuple>::type _M_invoke(_Index_tuple<_Ind...>) { return std::__invoke(std::get<_Ind>(std::move(_M_t))...); }

        typename __result<_Tuple>::type operator()() {
            using _Indices = typename _Build_index_tuple<tuple_size<_Tuple>::value>::__type;
            return _M_invoke(_Indices());
        }
    };

public:
    template<typename... _Tp> 
    using _Call_wrapper = _Invoker<tuple<typename decay<_Tp>::type...>>;

};

// th.join() th.detach() emm比较重要
inline void thread::join() { 
    int __e = EINVAL;
    if (_M_id != id()) __e = __gthread_join(_M_id._M_thread, 0);
    if (__e) __throw_system_error(__e);

    _M_id = id();
}
inline void thread::detach() {
    int __e = EINVAL;
    if (_M_id != id()) __e = __gthread_detach(_M_id._M_thread);
    if (__e) __throw_system_error(__e);

    _M_id = id();
}
inline unsigned int thread::hardware_concurrency() noexcept { return 0; }

inline void swap(thread& __x, thread& __y) noexcept { __x.swap(__y); }
inline bool operator==(thread::id __x, thread::id __y) noexcept { return __x._M_thread == __y._M_thread; }

template<>
struct hash<thread::id> : public __hash_base<size_t, thread::id> {
    size_t operator()(const thread::id& __id) const noexcept { 
        return std::_Hash_impl::hash(__id._M_thread); 
    }
};

namespace this_thread {
    inline thread::id get_id() noexcept {
    #ifdef _GLIBCXX_NATIVE_THREAD_ID
        return thread::id(_GLIBCXX_NATIVE_THREAD_ID);
    #else 
        return thread::id(__gthread_self());
    #endif
    }

    inline void yield() noexcept { // 礼貌的使自身挂起()
    #ifdef _GLIBCXX_USE_SCHED_YIELD
        __gthread_yield();
    #endif
    }

    // in another file
    template<typename _Rep, typename _Period>
    inline void sleep_for(const chrono::duration<_Rep, _Period>& __rtime) {
        if (__rtime <= __rtime.zero()) return;

        auto __s = chrono::duration_cast<chrono::seconds>(__rtime);
        auto __ns = chrono::duration_cast<chrono::nanoseconds>(__rtime - __s);

        struct ::timespec __ts = {
            static_cast<std::time_t>(__s.count()),
            static_cast<long>(__ns.count())
        };
        while (::nanosleep(&__ts, &__ts) == -1 && errno == EINTR);
    }

    template<typename _Clock, typename _Duration>
    inline void sleep_until(const chrono::time_point<_Clock, _Duration>& __atime) {
        static_assert(chrono::is_clock_v<_Clock>);

        auto __now = _Clock::now();
        if (_Clock::is_steady) {
            if (__now < __atime) sleep_for(__atime - __now);
            return;
        }
        while (__now < __atime) {
            sleep_for(__atime - __now);
            __now = _Clock::now();
        }
    }
}

}
