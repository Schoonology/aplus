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
    this->fulfillment = Persistent<Array>::New(Array::New());
    this->rejection = Persistent<Array>::New(Array::New());
  }

  Promise::~Promise() {
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
      Handle<Value> argv[1] = { args[0] };
      return constructor->NewInstance(1, argv);
    }

    // Creates a new instance object of this type and wraps it.
    Promise* obj = new Promise();
    obj->Wrap(args.This());

    // Check for value or reason arguments.
    if (args.Length() == 1) {
      obj->state = PROMISE_FULFILLED;
      obj->value = Persistent<Value>::New(args[0]);
    } else if (args.Length() == 2) {
      if (!args[0]->IsNull() && !args[0]->IsUndefined()) {
        THROW("Both value and reason were provided to Promise; they are mutually exclusive.");
      }

      obj->state = PROMISE_REJECTED;
      obj->reason = Persistent<Value>::New(args[1]);
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

    if (args.Length() >= 1 && args[0]->IsFunction()) {
      if (self->state == PROMISE_PENDING) {
        PUSH(self->fulfillment, args[0]->ToObject());
      } else if (self->state == PROMISE_FULFILLED) {
        Handle<Value> newargs[] = { self->value };
        args[0]->ToObject()->CallAsFunction(Object::New(), 1, newargs);
      }
    }

    if (args.Length() >= 2 && args[1]->IsFunction()) {
      if (self->state == PROMISE_PENDING) {
        PUSH(self->rejection, args[1]->ToObject());
      } else if (self->state == PROMISE_REJECTED) {
        Handle<Value> newargs[] = { self->reason };
        args[1]->ToObject()->CallAsFunction(Object::New(), 1, newargs);
      }
    }

    return scope.Close(args.This());
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
      THROW("Promise already fulfilled or rejected, and is immutable.");
    }

    Handle<Value> value;

    if (args.Length() > 0) {
      value = args[0];
    } else {
      value = Undefined();
    }

    self->state = PROMISE_FULFILLED;
    self->value = Persistent<Value>::New(value);

    int length = self->fulfillment->Length();
    Handle<Value> newargs[] = { value };

    for (int i = 0; i < length; i++) {
      // TODO: What should `this` be?
      self->fulfillment->Get(i)->ToObject()->CallAsFunction(Object::New(), 1, newargs);
    }

    return scope.Close(Undefined());
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
      THROW("Promise already fulfilled or rejected, and is immutable.");
    }

    Handle<Value> reason;

    if (args.Length() > 0) {
      reason = args[0];
    } else {
      reason = Undefined();
    }

    self->state = PROMISE_REJECTED;
    self->reason = Persistent<Value>::New(reason);

    int length = self->rejection->Length();
    Handle<Value> newargs[] = { reason };

    for (int i = 0; i < length; i++) {
      // TODO: What should `this` be?
      self->rejection->Get(i)->ToObject()->CallAsFunction(Object::New(), 1, newargs);
    }

    return scope.Close(Undefined());
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
