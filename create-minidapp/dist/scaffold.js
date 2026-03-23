"use strict";
/**
 * scaffold.ts — core scaffolding logic
 * Renders templates and writes project files to disk
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.scaffold = scaffold;
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const templates_1 = require("./templates");
function scaffold(options) {
    const { projectName, template, outputDir } = options;
    const tpl = templates_1.TEMPLATES[template];
    if (!tpl) {
        throw new Error(`Unknown template: "${template}". Run with --list to see available templates.`);
    }
    const projectDir = path.resolve(outputDir, projectName);
    if (fs.existsSync(projectDir)) {
        throw new Error(`Directory already exists: ${projectDir}`);
    }
    // Create project directory
    fs.mkdirSync(projectDir, { recursive: true });
    const replacements = {
        '{{NAME}}': projectName,
        '{{NAME_LOWER}}': projectName.toLowerCase().replace(/\s+/g, '-'),
        '{{DESCRIPTION}}': `A MiniDapp called ${projectName}`,
        '{{RECV_AMOUNT}}': '100',
        '{{RECV_ADDRESS}}': '0x00',
        '{{RECV_TOKEN}}': '0x00',
    };
    let filesWritten = 0;
    for (const file of tpl.files) {
        const filePath = path.join(projectDir, file.path);
        const fileDir = path.dirname(filePath);
        // Ensure subdirectories exist
        if (!fs.existsSync(fileDir)) {
            fs.mkdirSync(fileDir, { recursive: true });
        }
        // Apply replacements
        let content = file.content;
        for (const [key, val] of Object.entries(replacements)) {
            content = content.split(key).join(val);
        }
        fs.writeFileSync(filePath, content, 'utf8');
        filesWritten++;
    }
    console.log(`\n✅ Created MiniDapp project: ${projectDir}`);
    console.log(`   Template : ${template}`);
    console.log(`   Files    : ${filesWritten}`);
    console.log(`\nNext steps:`);
    console.log(`  cd ${projectName}`);
    console.log(`  # Replace mds.js with the real file from your Minima node`);
    console.log(`  # http://localhost:9003/mds.js`);
    console.log(`  npm run package   # creates ${projectName.toLowerCase()}.mds.zip`);
    console.log(`  # Install the .mds.zip on your node`);
    console.log('');
}
//# sourceMappingURL=scaffold.js.map