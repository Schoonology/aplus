var aplus = require('bindings')('aplus.node')
  , PENDING = 0
  , FULFILLED = 1
  , REJECTED = 2

//
// # Promise
//
function Promise(value, reason) {
  if (!(this instanceof Promise)) {
    return new Promise(value, reason)
  }

  this.state = PENDING
  this.value = null
  this.reason = null
  this.children = []

  if (arguments.length === 1) {
    this.fulfill(value)
  } else if (arguments.length === 2) {
    if (value == null) {
      this.reject(reason)
    } else {
      throw new Error('Both value and reason were provided to Promise; they are mutually exclusive.')
    }
  }

  if (Promise.super_) {
    Promise.super_.call(this, value, reason)
  }
}

//
// ## notify `notify(child)`
//
// TODO: Description.
//
Promise.prototype.notify = notify
function notify(child) {
  var self = this
    , result
    , fn
    , arg

  switch (self.state) {
    case FULFILLED:
      fn = child.fulfilled
      arg = self.value

      if (!fn) {
        child.promise.fulfill(arg)
        return
      }

      break
    case REJECTED:
      fn = child.rejected
      arg = self.reason

      if (!fn) {
        child.promise.reject(arg)
        return
      }

      break
    default:
      return
  }

  try {
    child.promise.fulfill(fn(arg))
  } catch (e) {
    child.promise.reject(e)
  }
}

//
// ## then `then(fulfilled, rejected)`
//
// TODO: Description.
//
Promise.prototype.then = then
function then(fulfilled, rejected) {
  var self = this
    , child = {}

  child.promise = new Promise()
  child.fulfilled = typeof fulfilled === 'function' ? fulfilled : null
  child.rejected = typeof rejected === 'function' ? rejected : null

  if (self.state === PENDING) {
    self.children.push(child)
    return child.promise
  }

  process.nextTick(function () {
    self.notify(child)
  })

  return child.promise
}

//
// ## fulfill `fulfill(value)`
//
// TODO: Description.
//
Promise.prototype.fulfill = fulfill
function fulfill(value) {
  var self = this

  if (self.state !== PENDING) {
    return false
  }

  if (value && typeof value === 'object' && typeof value.then === 'function') {
    value.then(function (value) {
      self.fulfill(value)
    }, function (reason) {
      self.reject(reason)
    })
    return
  }

  self.state = FULFILLED
  self.value = value

  self.children.forEach(function (child) {
    self.notify(child)
  })

  return true
}

//
// ## reject `reject(reason)`
//
// TODO: Description.
//
Promise.prototype.reject = reject
function reject(reason) {
  var self = this

  if (self.state !== PENDING) {
    return false
  }

  self.state = REJECTED
  self.reason = reason

  self.children.forEach(function (child) {
    self.notify(child)
  })

  return true
}

// TODO: Remove.
// aplus.Promise = Promise
module.exports = aplus
