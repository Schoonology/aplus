var aplus = require('../../lib/aplus')

// fulfilled(value): creates a promise that is already fulfilled with value.
function fulfilled(value) {
  return new aplus.Promise(value)
}

// rejected(reason): creates a promise that is already rejected with reason.
function rejected(reason) {
  return new aplus.Promise(null, reason)
}

// pending(): creates a tuple consisting of { promise, fulfill, reject }:
// promise is a promise object that is currently in the pending state.
// fulfill(value) moves the promise from the pending state to a fulfilled state, with fulfillment value value.
// reject(reason) moves the promise from the pending state to the rejected state, with rejection reason reason.
function pending() {
  var promise = new aplus.Promise()

  return {
    promise: promise,
    fulfill: function (value) {
      return promise.fulfill(value)
    },
    reject: function (value) {
      return promise.reject(value)
    }
  }
}

module.exports = {
  fulfilled: fulfilled,
  rejected: rejected,
  pending: pending
}
