/** Shapes returned by the backend. Mirrors `backend/app/schemas.py`. */

export type Role = 'admin' | 'editor' | 'reader';

export interface User {
  id: number;
  username: string;
  display_name: string;
  email: string;
  role: Role;
  is_active: boolean;
  created_at?: string | null;
  last_login_at?: string | null;
}

export interface Space {
  id: number;
  slug: string;
  title: string;
  summary: string;
  source_area: string;
  position: number;
}

export interface PageSummary {
  id: number;
  slug: string;
  title: string;
  summary: string;
  status: 'published' | 'draft';
  origin: 'seed' | 'authored';
  position: number;
  version: number;
  tags: string[];
  updated_at?: string | null;
  space?: Space | null;
}

export interface Page extends PageSummary {
  body: string;
  created_at?: string | null;
  created_by?: User | null;
  updated_by?: User | null;
}

export interface Revision {
  id: number;
  version: number;
  title: string;
  summary: string;
  comment: string;
  created_at?: string | null;
  author?: User | null;
}

export interface RevisionDetail extends Revision {
  body: string;
}

export type Implementation = 'c' | 'cpp' | 'x86-asm' | 'unresolved' | '';

export interface SymbolSummary {
  id: number;
  name: string;
  kind: string;
  area: string;
  file: string;
  line: number;
  signature: string;
  implementation: Implementation;
  is_codol: boolean;
}

export interface SymbolMember {
  name: string;
  type: string;
  comment: string;
  bits: string;
}

export interface SymbolParam {
  type: string;
  name: string;
}

export interface SymbolDetail extends SymbolSummary {
  end_line: number;
  doc: string;
  declaration: string;
  detail: {
    members?: SymbolMember[];
    values?: { name: string; value: string; comment: string }[];
    methods?: string[];
    params?: SymbolParam[];
    returns?: string;
    storage?: string;
    underlying?: string;
    tag?: string;
    aliases?: string[];
    bases?: string;
    alias_of?: { name: string; kind: string; file: string; line: number };
    defined_in?: { file: string; line: number; language: string }[];
    implementation?: string;
    implementation_note?: string;
    language?: string;
    retained?: boolean;
    exported?: boolean;
    instruction_lines?: number;
    imports?: string[];
    value?: string;
    parameters?: SymbolParam[];
    callable?: boolean;
  };
}

export interface Builtin {
  id: number;
  name: string;
  kind: 'function' | 'method';
  binding: string;
  area: string;
  implementation: Implementation;
  developer_only: boolean;
  registered_file: string;
  registered_line: number;
  binding_file: string;
  binding_line: number;
  signature: string;
  doc: string;
}

export interface Area {
  name: string;
  title: string;
  summary: string;
  path: string;
  is_codol: boolean;
  file_count: number;
  line_count: number;
  symbol_count: number;
  languages: Record<string, number>;
  position: number;
}

export interface SourceFile {
  path: string;
  area: string;
  language: string;
  role: string;
  lines: number;
  bytes: number;
  sha256: string;
  symbol_count: number;
}

export interface Paged<T> {
  total: number;
  limit: number;
  offset: number;
  items: T[];
}

export interface SearchHit {
  type: 'page' | 'symbol' | 'builtin';
  title: string;
  subtitle: string;
  href: string;
  snippet: string;
  rank: number;
}

export interface SearchResponse {
  query: string;
  hits: SearchHit[];
  counts: Record<string, number>;
}

export interface IndexRun {
  id: number;
  started_at?: string | null;
  finished_at?: string | null;
  repo_root: string;
  git_commit: string;
  files: number;
  symbols: number;
  builtins: number;
  summary: Record<string, unknown>;
}

export interface SiteStats {
  site_name: string;
  tagline: string;
  pages: number;
  spaces: number;
  symbols: number;
  builtins: number;
  files: number;
  lines: number;
  implementations: Record<string, number>;
  last_index?: IndexRun | null;
}
