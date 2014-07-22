
#ifndef ASYNC_HPP
#define ASYNC_HPP

#include <type_traits>

namespace Async {

template<typename Signature>
class Callback;

template<typename Res, typename... Args>
class Callback<Res(Args...)> {
public:
	template<Res (*function)(Args...)>
	static Callback<Res(Args...)> make() {
		struct Wrapper {
			static Res run(void *object, Args... args) {
				return function(args...);
			}
		};
		return Callback(nullptr, Wrapper::run);
	}

	template<typename Object, Res (Object::*function)(Args...)>
	static Callback<Res(Args...)> make(Object *object) {
		struct Wrapper {
			static Res run(void *object, Args... args) {
				auto ptr = static_cast<Object *>(object);
				return (ptr->*function)(args...);
			}
		};
		return Callback(object, Wrapper::run);
	}

	Callback() : p_object(nullptr), p_function(nullptr) { };

	Callback(void *object, Res (*function)(void *, Args...))
			: p_object(object), p_function(function) { }

	Res operator() (Args... args) const {
		return p_function(p_object, args...);
	}
	
private:
	void *p_object;
	Res (*p_function)(void *, Args...);
};

template<typename F, F function>
struct Member;

template<typename Object, typename Res, typename... Args, Res(Object::*function)(Args...)>
struct Member<Res(Object::*)(Args...), function> {
	static Callback<Res(Args...)> make(Object *object) {
		return Callback<Res(Args...)>::template make<Object, function>(object);
	}
};

#define ASYNC_MEMBER(x, f) ::Async::Member<decltype(f), (f)>::make(x)

template<typename FinalCb, int i, typename... Functors>
struct p_staticSeries;

template<typename FinalCb, typename... Functors>
struct p_staticSeriesTail {
	static void invoke(std::tuple<Functors...> functors, FinalCb callback) {
		callback(Error(true));
	}
};

template<typename FinalCb, int i, typename... Functors>
struct p_staticSeriesItem {
	static void invoke(std::tuple<Functors...> functors, FinalCb callback) {
		auto functor = std::get<i>(functors);
		functor([functors, callback] (Error error) {
			if(!error.ok())
				return callback(error);
			p_staticSeries<FinalCb, i + 1, Functors...>::invoke(functors, callback);
		});
	}
};

template<typename FinalCb, int i, typename... Functors>
struct p_staticSeries {
	static void invoke(std::tuple<Functors...> functors,
			FinalCb callback) {
		std::conditional<i == sizeof...(Functors),
				p_staticSeriesTail<FinalCb, Functors...>,
				p_staticSeriesItem<FinalCb, i, Functors...>>
			::type::invoke(functors, callback);
	}
};

template<typename... Functors, typename FinalCb>
void staticSeries(std::tuple<Functors...> functors, FinalCb callback) {
	p_staticSeries<FinalCb, 0, Functors...>::invoke(functors, callback);
}

template<typename Iterator, typename Functor, typename FinalCb>
void eachSeries(Iterator begin, Iterator end, Functor functor,
		FinalCb callback) {
	if(begin == end) {
		callback(Error(true));
		return;
	}
	functor(*begin, [begin, end, functor, callback](Error error) {
		if(!error.ok())
			return callback(error);
		eachSeries(std::next(begin), end, functor, callback);
	});
}

template<typename Iterator, typename FinalCb>
void seriesIter(Iterator begin, Iterator end,
		FinalCb callback) {
	struct Control {
		Control(Iterator start, Iterator end, FinalCb callback)
				: p_current(start), p_end(end), p_callback(callback) {
			p_recurse = [this] () { (*this)(); };
		}

		void operator() () {
			if(p_current == p_end) {
				p_callback();
			}else{
				Iterator functor = p_current;
				p_current++;
				(*functor)(p_recurse);
			}
		}
		
		std::function<void()> p_recurse;
		Iterator p_current;
		Iterator p_end;
		FinalCb p_callback;
	};
	
	Control *control = new Control(begin, end, callback);
	(*control)();
}

template<typename Test, typename Functor, typename FinalCb>
void whilst(Test test, Functor functor,
		FinalCb callback) {
	struct Control {
		Control(Test test, Functor functor, FinalCb callback)
				: p_test(test), p_functor(functor), p_callback(callback) {
			p_recurse = [this] (Error error) {
				if(!error.ok())
					return p_callback(error);
				(*this)();
			};
		}

		void operator() () {
			if(!p_test()) {
				p_callback(Error(true));
				delete this;
			}else{
				p_functor(p_recurse);
			}
		}
		
		std::function<void(Error)> p_recurse;
		Test p_test;
		Functor p_functor;
		FinalCb p_callback;
	};
	
	Control *control = new Control(test, functor, callback);
	(*control)();
}

};

#endif

