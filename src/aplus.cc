#include <node.h>
#include <stdio.h>

#include "aplus.h"

using namespace v8;
using namespace node;

namespace aplus {
  Persistent<Function> Promise::constructor;

  //
  // ## Helpers
  //
  // These "throw" helpers bail the calling context immediately.
  //
  #define THROW(str) return ThrowException(Exception::Error(String::New(str)));
  #define THROW_REF(str) return ThrowException(Exception::ReferenceError(String::New(str)));
  #define THROW_TYPE(str) return ThrowException(Exception::TypeError(String::New(str)));

  //
  // PUSH shorthand for v8::Array.
  //
  #define PUSH(a, v) a->Set(a->Length(), v);

  //
  // Internal versions of Unwrap.
  //
  #define PROMISE_TO_THIS(obj) obj->handle_
  ; // For Sublime's syntax highlighter. Ignore.
  #define THIS_TO_PROMISE(obj) ObjectWrap::Unwrap<Promise>(obj)

  //
  // ## Promise
  //
  Promise::Promise() : ObjectWrap(), state(PROMISE_PENDING) {
    this->children = Persistent<Array>::New(Array::New());

    assert(uv_idle_init(uv_default_loop(), &handle) == 0);
    handle.data = this;
  }

  Promise::~Promise() {
    // TODO: Cleanup.
    // TODO: Guarantee this can be called.
  }

  //
  // ## Promise(value, reason)
  //
  // Creates a new Promise. If value is provided, the Promise starts in the fulfilled state. If reason is provided,
  // the Promise starts in the rejected state. If neither is provided, the Promise starts in the pending state.
  // If both are provided, throws an Error.
  //
  Handle<Value> Promise::New(const Arguments& args) {
    HandleScope scope;

    if (!args.IsConstructCall()) {
      Handle<Value> argv[2] = { args[0], args[1] };
      return constructor->NewInstance(2, argv);
    }

    // Creates a new instance object of this type and wraps it.
    Promise* obj = new Promise();
    obj->Wrap(args.This());

    // Check for value or reason arguments.
    if (args.Length() == 1) {
      Handle<Value> subargs[1] = { args[0] };
      args.This()->Get(String::NewSymbol("fulfill"))->ToObject()->CallAsFunction(args.This(), 1, subargs);
    } else if (args.Length() == 2) {
      if (!args[0]->IsNull() && !args[0]->IsUndefined()) {
        THROW("Both value and reason were provided to Promise; they are mutually exclusive.");
      }

      Handle<Value> subargs[1] = { args[1] };
      args.This()->Get(String::NewSymbol("reject"))->ToObject()->CallAsFunction(args.This(), 1, subargs);
    }

    // If we've had a super"class" provided, be sure to call its constructor.
    Handle<Value> super = constructor->Get(String::NewSymbol("super_"));
    if (super->IsFunction()) {
      super->ToObject()->CallAsFunction(args.This(), 0, NULL);
    }

    return args.This();
  }

  //
  // ## Then `Then(fulfilled, rejected)`
  //
  // Registers **fulfilled** as a fulfillment callback and **rejected** as a rejection callback.
  //
  Handle<Value> Promise::Then(const Arguments& args) {
    HandleScope scope;
    Promise* self = THIS_TO_PROMISE(args.This());
    assert(self);

    Handle<Object> child = Object::New();
    Handle<Object> promise = constructor->NewInstance(0, NULL);

    child->Set(String::NewSymbol("promise"), promise);

    if (args.Length() >= 1 && args[0]->IsFunction()) {
      child->Set(String::NewSymbol("fulfilled"), args[0]);
    } else {
      child->Set(String::NewSymbol("fulfilled"), Null());
    }

    if (args.Length() >= 2 && args[1]->IsFunction()) {
      child->Set(String::NewSymbol("rejected"), args[1]);
    } else {
      child->Set(String::NewSymbol("rejected"), Null());
    }

    PUSH(self->children, child);

    if (self->state != PROMISE_PENDING && !uv_is_active((const uv_handle_t*)&self->handle)) {
      uv_idle_start(&self->handle, NotifyLater);
    }

    return scope.Close(promise);
  }

  //
  // ## Fulfill `Fulfill(value)`
  //
  // Attempts to fulfill the Promise with **value**. If the Promise has already been fulfilled or rejected, throws
  // an Error. If successful, all fulfillment callbacks will be fired in order.
  //
  Handle<Value> Promise::Fulfill(const Arguments& args) {
    HandleScope scope;
    Promise* self = THIS_TO_PROMISE(args.This());
    assert(self);

    if (self->state != PROMISE_PENDING) {
      return scope.Close(Boolean::New(0));
    }

    Handle<Value> value;

    if (args.Length() > 0) {
      value = args[0];
    } else {
      value = Undefined();
    }

    if (value->IsObject()) {
      Handle<Value> then = value->ToObject()->Get(String::NewSymbol("then"));

      if (then->IsFunction()) {
    // TODO
  //   value.then(function (value) {
  //     self.fulfill(value)
  //   }, function (reason) {
  //     self.reject(reason)
  //   })
        return scope.Close(Boolean::New(1));
      }
    }

    self->state = PROMISE_FULFILLED;
    self->value = Persistent<Value>::New(value);

    int length = self->children->Length();
    Handle<Value> subargs[1];

    for (int i = 0; i < length; i++) {
      subargs[0] = self->children->Get(i)->ToObject();
      args.This()->Get(String::NewSymbol("notify"))->ToObject()->CallAsFunction(args.This(), 1, subargs);
    }

    self->children.Dispose();
    self->children = Persistent<Array>::New(Array::New());

    return scope.Close(Boolean::New(1));
  }

  //
  // ## Reject `Reject(reason)`
  //
  // Attempts to reject the Promise with **reason**. If the Promise has already been fulfilled or rejected, throws
  // an Error. If successful, all rejection callbacks will be fired in order.
  //
  Handle<Value> Promise::Reject(const Arguments& args) {
    HandleScope scope;
    Promise* self = THIS_TO_PROMISE(args.This());
    assert(self);

    if (self->state != PROMISE_PENDING) {
      return scope.Close(Boolean::New(0));
    }

    Handle<Value> reason;

    if (args.Length() > 0) {
      reason = args[0];
    } else {
      reason = Undefined();
    }

    self->state = PROMISE_REJECTED;
    self->reason = Persistent<Value>::New(reason);

    int length = self->children->Length();
    Handle<Value> subargs[1];

    for (int i = 0; i < length; i++) {
      subargs[0] = self->children->Get(i)->ToObject();
      args.This()->Get(String::NewSymbol("notify"))->ToObject()->CallAsFunction(args.This(), 1, subargs);
    }

    self->children.Dispose();
    self->children = Persistent<Array>::New(Array::New());

    return scope.Close(Boolean::New(1));
  }

  //
  // ## Notify `Notify()`
  //
  // For internal use only.
  //
  // Cycles through all rejection or fulfillment callbacks, in order, firing as necessary.
  //
  // Returns nothing.
  //
  Handle<Value> Promise::Notify(const Arguments& args) {
    HandleScope scope;
    Promise* self = THIS_TO_PROMISE(args.This());
    assert(self);

    Handle<Object> child = args[0]->ToObject();
    Handle<Object> promise = child->Get(String::NewSymbol("promise"))->ToObject();
    Handle<Value> notifyFn;
    Handle<Value> notifyArgs[1];

    switch (self->state) {
      case PROMISE_FULFILLED:
        notifyFn = child->Get(String::NewSymbol("fulfilled"));
        notifyArgs[0] = self->value;

        if (!notifyFn->IsObject()) {
          promise->Get(String::NewSymbol("fulfill"))->ToObject()->CallAsFunction(promise, 1, notifyArgs);
          return scope.Close(Undefined());
        }
        break;
      case PROMISE_REJECTED:
        notifyFn = child->Get(String::NewSymbol("rejected"));
        notifyArgs[0] = self->reason;

        if (!notifyFn->IsObject()) {
          promise->Get(String::NewSymbol("reject"))->ToObject()->CallAsFunction(promise, 1, notifyArgs);
          return scope.Close(Undefined());
        }
        break;
    }

    TryCatch trycatch;
    notifyArgs[0] = notifyFn->ToObject()->CallAsFunction(Object::New(), 1, notifyArgs);

    if (trycatch.HasCaught()) {
      notifyArgs[0] = trycatch.Exception();
      promise->Get(String::NewSymbol("reject"))->ToObject()->CallAsFunction(promise, 1, notifyArgs);
    } else {
      promise->Get(String::NewSymbol("fulfill"))->ToObject()->CallAsFunction(promise, 1, notifyArgs);
    }

    return scope.Close(Undefined());
  }

  //
  // ## NotifyLater
  //
  void Promise::NotifyLater(uv_idle_t* handle, int status) {
    assert(handle);

    Promise* self = (Promise*)handle->data;
    assert(self);

    Handle<Value> jsObj = PROMISE_TO_THIS(self);
    if (!jsObj->IsObject()) {
      return;
    }

    int length = self->children->Length();
    Handle<Value> subargs[1];

    for (int i = 0; i < length; i++) {
      subargs[0] = self->children->Get(i)->ToObject();
      jsObj->ToObject()->Get(String::NewSymbol("notify"))->ToObject()->CallAsFunction(jsObj->ToObject(), 1, subargs);
    }

    self->children.Dispose();
    self->children = Persistent<Array>::New(Array::New());

    uv_idle_stop(handle);
  }

  //
  // ## Initialize
  //
  // Creates and populates the constructor Function and its prototype.
  //
  void Promise::Initialize() {
    Local<FunctionTemplate> constructorTemplate(FunctionTemplate::New(New));

    // ObjectWrap uses the first internal field to store the wrapped pointer.
    constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
    constructorTemplate->SetClassName(String::NewSymbol("Promise"));

    // Add all prototype methods, getters and setters here.
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "then", Then);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "fulfill", Fulfill);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "reject", Reject);
    NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "notify", Notify);

    constructor = Persistent<Function>::New(constructorTemplate->GetFunction());
  }

  //
  // ## InstallExports
  //
  // Exports the Promise class within the module `target`.
  //
  void Promise::InstallExports(Handle<Object> target) {
    HandleScope scope;

    Initialize();

    // This has to be last, otherwise the properties won't show up on the object in JavaScript.
    target->Set(String::NewSymbol("Promise"), constructor);
  }

  NODE_MODULE(aplus, Promise::InstallExports);
}
