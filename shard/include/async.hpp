
#ifndef ASYNC_HPP
#define ASYNC_HPP

#include <libchain/all.hpp>

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
	static Callback<Res(Args...)> makeMember(Object *object) {
		struct Wrapper {
			static Res run(void *object, Args... args) {
				auto ptr = static_cast<Object *>(object);
				return (ptr->*function)(args...);
			}
		};
		return Callback(object, Wrapper::run);
	}
	
	template<typename Object, Res (*function)(Object *, Args...)>
	static Callback<Res(Args...)> makeStatic(Object *object) {
		struct Wrapper {
			static Res run(void *object, Args... args) {
				auto ptr = static_cast<Object *>(object);
				return function(ptr, args...);
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
		return Callback<Res(Args...)>::template makeMember<Object, function>(object);
	}
};

#define ASYNC_MEMBER(x, f) ::Async::Member<decltype(f), (f)>::make(x)

template<typename... Args>
Callback<void(Args...)> transition(libchain::Callback<void(Args...)> callback) {
	return Callback<void(Args...)>::template makeStatic<
		typename libchain::Callback<void(Args...)>::Interface,
		&libchain::Callback<void(Args...)>::invoke
	>(callback.implementation());
}

};

#endif

