#!/usr/bin/env node
"use strict";
/**
 * create-minidapp CLI
 * Usage: create-minidapp <project-name> [--template <name>] [--list]
 */
Object.defineProperty(exports, "__esModule", { value: true });
const scaffold_1 = require("./scaffold");
const templates_1 = require("./templates");
const args = process.argv.slice(2);
function printHelp() {
    console.log(`
  create-minidapp — Scaffold CLI for Minima MiniDapps

  Usage:
    create-minidapp <project-name> [options]

  Options:
    --template <name>  Template to use (default: basic)
    --list             List available templates
    --help             Show this help

  Examples:
    create-minidapp my-wallet
    create-minidapp my-swap --template exchange
    create-minidapp counter-app --template counter
`);
}
function parseArgs(argv) {
    const result = { name: undefined, template: 'basic', list: false, help: false };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        if (arg === '--list') {
            result.list = true;
            continue;
        }
        if (arg === '--help' || arg === '-h') {
            result.help = true;
            continue;
        }
        if (arg === '--template' || arg === '-t') {
            result.template = argv[++i] || 'basic';
            continue;
        }
        if (!arg.startsWith('-')) {
            result.name = arg;
        }
    }
    return result;
}
function main() {
    const opts = parseArgs(args);
    if (opts.help || args.length === 0) {
        printHelp();
        process.exit(0);
    }
    if (opts.list) {
        (0, templates_1.listTemplates)();
        process.exit(0);
    }
    if (!opts.name) {
        console.error('Error: project name is required\n');
        printHelp();
        process.exit(1);
    }
    if (!/^[a-zA-Z0-9_-]+$/.test(opts.name)) {
        console.error(`Error: invalid project name "${opts.name}"`);
        console.error('Use only letters, numbers, hyphens, and underscores.');
        process.exit(1);
    }
    try {
        (0, scaffold_1.scaffold)({
            projectName: opts.name,
            template: opts.template,
            outputDir: process.cwd(),
        });
    }
    catch (err) {
        console.error(`Error: ${err.message}`);
        process.exit(1);
    }
}
main();
//# sourceMappingURL=cli.js.map