import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import rehypeSlug from 'rehype-slug';
import rehypeHighlight from 'rehype-highlight';

/**
 * Wiki prose. GFM gives tables and task lists, `rehype-slug` gives every
 * heading an id so the table of contents can link to it, and highlighting is
 * done at render time so no syntax theme has to be shipped to the browser.
 */
export function Markdown({ children }: { children: string }) {
  return (
    <div className="prose">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[rehypeSlug, [rehypeHighlight, { detect: true, ignoreMissing: true }]]}
      >
        {children}
      </ReactMarkdown>
    </div>
  );
}

export interface Heading {
  depth: number;
  text: string;
  id: string;
}

/** Headings for the table of contents, skipping anything inside a code fence. */
export function headings(markdown: string): Heading[] {
  const out: Heading[] = [];
  let fenced = false;
  for (const line of markdown.split('\n')) {
    if (/^\s*(```|~~~)/.test(line)) {
      fenced = !fenced;
      continue;
    }
    if (fenced) continue;
    const match = /^(#{2,3})\s+(.+?)\s*#*$/.exec(line);
    if (!match) continue;
    const text = match[2].replace(/[`*_]/g, '').trim();
    out.push({ depth: match[1].length, text, id: slug(text) });
  }
  return out;
}

/** Matches github-slugger, which is what rehype-slug uses. */
function slug(text: string): string {
  return text
    .toLowerCase()
    .trim()
    .replace(/[^\p{L}\p{N}\p{M}\- ]/gu, '')
    .replace(/ /g, '-');
}
