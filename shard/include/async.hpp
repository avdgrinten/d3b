
#ifndef ASYNC_HPP
#define ASYNC_HPP

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

};

#endif

