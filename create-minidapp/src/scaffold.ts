/**
 * scaffold.ts — core scaffolding logic
 * Renders templates and writes project files to disk
 */

import * as fs from 'fs';
import * as path from 'path';
import { ScaffoldOptions } from './types';
import { TEMPLATES } from './templates';

export function scaffold(options: ScaffoldOptions): void {
  const { projectName, template, outputDir } = options;

  const tpl = TEMPLATES[template];
  if (!tpl) {
    throw new Error(
      `Unknown template: "${template}". Run with --list to see available templates.`
    );
  }

  const projectDir = path.resolve(outputDir, projectName);

  if (fs.existsSync(projectDir)) {
    throw new Error(`Directory already exists: ${projectDir}`);
  }

  // Create project directory
  fs.mkdirSync(projectDir, { recursive: true });

  const replacements: Record<string, string> = {
    '{{NAME}}':       projectName,
    '{{NAME_LOWER}}': projectName.toLowerCase().replace(/\s+/g, '-'),
    '{{DESCRIPTION}}': `A MiniDapp called ${projectName}`,
    '{{RECV_AMOUNT}}': '100',
    '{{RECV_ADDRESS}}': '0x00',
    '{{RECV_TOKEN}}':   '0x00',
  };

  let filesWritten = 0;

  for (const file of tpl.files) {
    const filePath = path.join(projectDir, file.path);
    const fileDir  = path.dirname(filePath);

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
