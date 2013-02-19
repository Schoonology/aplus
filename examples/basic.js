var aplus = require('../'),
    promise

function fulfilled(value) {
  console.log('Fulfilled:', value)
}

function rejected(reason) {
  console.log('Rejected:', reason)
}

promise = new aplus.Promise()
promise.then(fulfilled, rejected)
promise.fulfill('Hi!')

promise = new aplus.Promise()
promise.then(fulfilled, rejected)
promise.reject('Boo!')

promise = new aplus.Promise()
promise.fulfill('Hi again!')
promise.then(fulfilled, rejected)

promise = new aplus.Promise()
promise.reject('Boo again!')
promise.then(fulfilled, rejected)
