"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Environment = exports.KissVMInterpreter = exports.defaultTransaction = exports.MiniValue = exports.runSuites = exports.runScript = exports.expect = exports.test = exports.it = exports.describe = void 0;
var api_1 = require("./api");
Object.defineProperty(exports, "describe", { enumerable: true, get: function () { return api_1.describe; } });
Object.defineProperty(exports, "it", { enumerable: true, get: function () { return api_1.it; } });
Object.defineProperty(exports, "test", { enumerable: true, get: function () { return api_1.test; } });
Object.defineProperty(exports, "expect", { enumerable: true, get: function () { return api_1.expect; } });
Object.defineProperty(exports, "runScript", { enumerable: true, get: function () { return api_1.runScript; } });
Object.defineProperty(exports, "runSuites", { enumerable: true, get: function () { return api_1.runSuites; } });
Object.defineProperty(exports, "MiniValue", { enumerable: true, get: function () { return api_1.MiniValue; } });
Object.defineProperty(exports, "defaultTransaction", { enumerable: true, get: function () { return api_1.defaultTransaction; } });
var interpreter_1 = require("./interpreter");
Object.defineProperty(exports, "KissVMInterpreter", { enumerable: true, get: function () { return interpreter_1.KissVMInterpreter; } });
var environment_1 = require("./interpreter/environment");
Object.defineProperty(exports, "Environment", { enumerable: true, get: function () { return environment_1.Environment; } });
//# sourceMappingURL=index.js.map