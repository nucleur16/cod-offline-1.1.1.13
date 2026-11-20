import Link from 'next/link';
import { api, query } from '@/lib/api';
import type { Area, Paged, PageSummary, Space } from '@/lib/types';
import { NavLink } from './NavLink';

/** Left rail: spaces with their pages, then the generated reference. */
export async function Sidebar() {
  let spaces: Space[] = [];
  let pages: PageSummary[] = [];
  let areas: Area[] = [];
  let offline = false;

  try {
    [spaces, pages, areas] = await Promise.all([
      api<Space[]>('/spaces'),
      api<Paged<PageSummary>>('/pages' + query({ limit: 500 })).then((p) => p.items),
      api<Area[]>('/reference/areas'),
    ]);
  } catch {
    offline = true;
  }

  const codolAreas = areas.filter((area) => area.is_codol);

  return (
    <nav className="sidebar">
      <Link className="brand" href="/">
        sh1x server wiki
      </Link>
      <div className="brand-sub">Call of Duty Online Closed Alpha Server — a CoD4x fork</div>

      {offline && (
        <div className="notice error small">
          The API is not reachable. Start the backend, then reload.
        </div>
      )}

      {spaces.map((space) => {
        const inSpace = pages.filter((page) => page.space?.slug === space.slug);
        if (!inSpace.length) return null;
        return (
          <div className="nav-group" key={space.slug}>
            <p className="nav-heading">{space.title}</p>
            {inSpace.map((page) => (
              <NavLink key={page.slug} href={`/wiki/${page.slug}`}>
                <span>{page.title}</span>
                {page.status === 'draft' && <span className="nav-count">draft</span>}
              </NavLink>
            ))}
          </div>
        );
      })}

      <div className="nav-group">
        <p className="nav-heading">Reference</p>
        <NavLink href="/reference">
          <span>Overview</span>
        </NavLink>
        <NavLink href="/reference/symbols">
          <span>Declarations</span>
        </NavLink>
        <NavLink href="/reference/gsc">
          <span>GSC builtins</span>
        </NavLink>
        <NavLink href="/reference/files">
          <span>Source files</span>
        </NavLink>
      </div>

      {codolAreas.length > 0 && (
        <div className="nav-group">
          <p className="nav-heading">Source areas</p>
          {codolAreas.map((area) => (
            <NavLink key={area.name} href={`/reference/areas/${area.name}`}>
              <span>{area.title}</span>
              <span className="nav-count">{area.symbol_count}</span>
            </NavLink>
          ))}
        </div>
      )}
    </nav>
  );
}
