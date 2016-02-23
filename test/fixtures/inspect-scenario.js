'use strict';

const common = require('../common');

function Class() {
  this.x = 1;
  this.y = 123.456;

  this.hashmap = {};
}

Class.prototype.method = function method() {
  return common.addon.method();
};

const c = new Class();

c.hashmap['some-key'] = 42;
c.hashmap['other-key'] = 'ohai';
c.hashmap['cons-string'] = 'this could be a bit smaller, but v8 wants big str.';
c.hashmap['cons-string'] += c.hashmap['cons-string'];
c.hashmap[0] = null;
c.hashmap[4] = undefined;
c.hashmap[23] = /regexp/;

let scopedVar = 'scoped value';

c.hashmap.scoped = function name() {
  return scopedVar;
};

c.method();
