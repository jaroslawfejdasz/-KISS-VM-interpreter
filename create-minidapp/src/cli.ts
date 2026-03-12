#!/usr/bin/env node
/**
 * create-minidapp CLI
 * Usage: npx create-minidapp [project-name] [--template basic|counter|exchange]
 */
import path from 'node:path';
import { scaffold, listTemplates, getTemplate } from './scaffold';

const CYAN  = '\x1b[36m';
const GREEN = '\x1b[32m';
const YELLOW = '\x1b[33m';
const RED   = '\x1b[31m';
const BOLD  = '\x1b[1m';
const DIM   = '\x1b[2m';
const RESET = '\x1b[0m';

function main() {
  const args = process.argv.slice(2);

  // Handle --help
  if (args.includes('--help') || args.includes('-h')) {
    printHelp();
    process.exit(0);
  }

  // Handle --list
  if (args.includes('--list')) {
    console.log(`\n${BOLD}Available templates:${RESET}\n`);
    for (const t of listTemplates()) {
      console.log(`  ${CYAN}${t.name.padEnd(12)}${RESET} ${DIM}${t.description}${RESET}`);
    }
    console.log('');
    process.exit(0);
  }

  // Parse project name
  let projectName = args.find(a => !a.startsWith('--'));
  if (!projectName) {
    console.error(`${RED}Error: project name is required.${RESET}\n`);
    printHelp();
    process.exit(1);
  }

  // Validate project name
  if (!/^[a-zA-Z0-9][a-zA-Z0-9\-_ ]*$/.test(projectName)) {
    console.error(`${RED}Error: project name can only contain letters, numbers, hyphens, underscores, and spaces.${RESET}`);
    process.exit(1);
  }

  // Parse --template
  const templateIdx = args.findIndex(a => a === '--template' || a === '-t');
  const templateName = templateIdx !== -1 ? args[templateIdx + 1] : 'basic';

  // Validate template exists
  try { getTemplate(templateName); } catch(e: any) {
    console.error(`${RED}Error: ${e.message}${RESET}`);
    process.exit(1);
  }

  // Output directory
  const dirIdx = args.findIndex(a => a === '--dir' || a === '-d');
  const outputDir = dirIdx !== -1
    ? path.resolve(args[dirIdx + 1])
    : path.resolve(process.cwd(), projectName.replace(/\s+/g, '-').toLowerCase());

  // Print banner
  console.log(`
${CYAN}${BOLD}  ╔═══════════════════════════╗
  ║   create-minidapp  v0.1.0  ║
  ║   Minima MiniDapp Scaffold  ║
  ╚═══════════════════════════╝${RESET}
`);
  console.log(`  ${DIM}Project:  ${RESET}${BOLD}${projectName}${RESET}`);
  console.log(`  ${DIM}Template: ${RESET}${BOLD}${templateName}${RESET}`);
  console.log(`  ${DIM}Output:   ${RESET}${BOLD}${outputDir}${RESET}\n`);

  // Run scaffold
  let written: string[];
  try {
    written = scaffold({ projectName, template: templateName, outputDir });
  } catch(e: any) {
    console.error(`${RED}Error: ${e.message}${RESET}`);
    process.exit(1);
  }

  // Success output
  console.log(`${GREEN}${BOLD}  ✓ Created ${written.length} files:${RESET}\n`);
  for (const f of written) {
    console.log(`    ${GREEN}+${RESET} ${f}`);
  }

  const dirName = path.basename(outputDir);
  console.log(`
${BOLD}  Next steps:${RESET}

    ${CYAN}cd ${dirName}${RESET}
    ${DIM}# Edit index.html, then zip and install:${RESET}
    ${CYAN}zip -r ${dirName}.mds.zip dapp.conf icon.png index.html mds.js${RESET}

${BOLD}  Test your contracts:${RESET}

    ${CYAN}npm install minima-test${RESET}
    ${CYAN}npx minima-test run tests/${RESET}

${BOLD}  Docs:${RESET} https://docs.minima.global/docs/development/minidapps/
`);
}

function printHelp() {
  console.log(`
${CYAN}${BOLD}  create-minidapp${RESET} — Scaffold a Minima MiniDapp

  ${BOLD}Usage:${RESET}
    npx create-minidapp <project-name> [options]

  ${BOLD}Options:${RESET}
    --template, -t  <name>  Template to use (default: basic)
    --dir,      -d  <path>  Output directory (default: ./<project-name>)
    --list                  List available templates
    --help, -h              Show this help

  ${BOLD}Examples:${RESET}
    npx create-minidapp my-wallet
    npx create-minidapp my-dex --template exchange
    npx create-minidapp my-counter --template counter

  ${BOLD}Templates:${RESET}`);

  for (const t of listTemplates()) {
    console.log(`    ${CYAN}${t.name.padEnd(12)}${RESET} ${t.description}`);
  }
  console.log('');
}

main();
