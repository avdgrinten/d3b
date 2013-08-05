
#include <type_traits>

namespace Async {

typedef std::function<void()> Callback;

template<typename Callback, int i, typename... Functors>
struct p_staticSeries;

template<typename Callback, typename... Functors>
struct p_staticSeriesTail {
	static void invoke(std::tuple<Functors...> functors, Callback callback) {
		callback(Error(true));
	}
};

template<typename Callback, int i, typename... Functors>
struct p_staticSeriesItem {
	static void invoke(std::tuple<Functors...> functors, Callback callback) {
		auto functor = std::get<i>(functors);
		functor([functors, callback] (Error error) {
			if(!error.ok())
				return callback(error);
			p_staticSeries<Callback, i + 1, Functors...>::invoke(functors, callback);
		});
	}
};

template<typename Callback, int i, typename... Functors>
struct p_staticSeries {
	static void invoke(std::tuple<Functors...> functors,
			Callback callback) {
		std::conditional<i == sizeof...(Functors),
				p_staticSeriesTail<Callback, Functors...>,
				p_staticSeriesItem<Callback, i, Functors...>>
			::type::invoke(functors, callback);
	}
};

template<typename... Functors, typename Callback>
void staticSeries(std::tuple<Functors...> functors, Callback callback) {
	p_staticSeries<Callback, 0, Functors...>::invoke(functors, callback);
}

template<typename Iterator, typename Functor, typename Callback>
void eachSeries(Iterator begin, Iterator end, Functor functor,
		Callback callback) {
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

template<typename Iterator, typename Callback>
void seriesIter(Iterator begin, Iterator end,
		Callback callback) {
	struct Control {
		Control(Iterator start, Iterator end, Callback callback)
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
		Callback p_callback;
	};
	
	Control *control = new Control(begin, end, callback);
	(*control)();
}

template<typename Test, typename Functor, typename Callback>
void whilst(Test test, Functor functor,
		Callback callback) {
	struct Control {
		Control(Test test, Functor functor, Callback callback)
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
		Callback p_callback;
	};
	
	Control *control = new Control(test, functor, callback);
	(*control)();
}

};

