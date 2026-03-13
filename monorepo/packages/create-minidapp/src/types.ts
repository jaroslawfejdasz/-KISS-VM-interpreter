/**
 * Types for create-minidapp scaffold CLI
 */

export interface Template {
  name: string;
  description: string;
  files: FileEntry[];
}

export interface FileEntry {
  path: string;
  content: string;
}

export interface ScaffoldOptions {
  projectName: string;
  template: string;
  outputDir: string;
}

export interface DappConf {
  name: string;
  icon: string;
  version: string;
  description: string;
  browser: string;
  service?: string;
  minify?: string;
  permission?: string;
}
