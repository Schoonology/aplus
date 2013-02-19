#ifndef APLUS_H
#define APLUS_H

#include <node.h>

#define PROMISE_PENDING   0
#define PROMISE_FULFILLED 1
#define PROMISE_REJECTED  2

namespace aplus {
  class Promise : public node::ObjectWrap {
    public:
      static v8::Persistent<v8::Function> constructor;

      //
      // ## Initialize
      //
      // Creates and populates the constructor Function and its prototype.
      //
      static void Initialize();

      //
      // ## InstallExports
      //
      // Exports the Promise class within the module `target`.
      //
      static void InstallExports(v8::Handle<v8::Object> target);

      //
      // ## WeakRefGBCallback
      //
      // Used for V8 to notify aplus when a Promise's value has been cleaned up.
      //
      static void WeakRefGBCallback(v8::Persistent<v8::Value> object, void *parameter);

      virtual ~Promise();

    protected:
      int state;
      v8::Persistent<v8::Value> value;
      v8::Persistent<v8::Value> reason;
      v8::Persistent<v8::Array> fulfillment;
      v8::Persistent<v8::Array> rejection;

      Promise();

      //
      // ## Promise(value, reason)
      //
      // Creates a new Promise. If value is provided, the Promise starts in the fulfilled state. If reason is provided,
      // the Promise starts in the rejected state. If neither is provided, the Promise starts in the pending state.
      // If both are provided, throws an Error.
      //
      static v8::Handle<v8::Value> New(const v8::Arguments& args);

      //
      // ## Then `Then(fulfilled, rejected)`
      //
      // Registers **fulfilled** as a fulfillment callback and **rejected** as a rejection callback.
      //
      static v8::Handle<v8::Value> Then(const v8::Arguments& args);

      //
      // ## Fulfill `Fulfill(value)`
      //
      // Attempts to fulfill the Promise with **value**. If the Promise has already been fulfilled or rejected, throws
      // an Error. If successful, all fulfillment callbacks will be fired in order.
      //
      static v8::Handle<v8::Value> Fulfill(const v8::Arguments& args);

      //
      // ## Reject `Reject(reason)`
      //
      // Attempts to reject the Promise with **reason**. If the Promise has already been fulfilled or rejected, throws
      // an Error. If successful, all rejection callbacks will be fired in order.
      //
      static v8::Handle<v8::Value> Reject(const v8::Arguments& args);
  };
}

#endif
