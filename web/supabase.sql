-- Jalankan di Supabase: SQL Editor -> New query -> Run

create table if not exists public.readings (
  id bigint generated always as identity primary key,
  created_at timestamptz not null default now(),
  device text,
  temperature real,
  humidity real,
  smoke real,
  flame real,
  status text,
  ts_ms bigint
);

create table if not exists public.alerts (
  id bigint generated always as identity primary key,
  created_at timestamptz not null default now(),
  device text,
  temperature real,
  smoke real,
  flame real,
  status text,
  telegram_ok boolean
);

create index if not exists readings_created_idx on public.readings (created_at desc);
create index if not exists alerts_created_idx on public.alerts (created_at desc);

-- RLS aktif: browser (anon key) hanya boleh MEMBACA.
-- Penulisan hanya lewat fungsi Vercel memakai service_role key (melewati RLS).
alter table public.readings enable row level security;
alter table public.alerts enable row level security;

drop policy if exists "anon read readings" on public.readings;
drop policy if exists "anon read alerts" on public.alerts;
create policy "anon read readings" on public.readings for select to anon using (true);
create policy "anon read alerts" on public.alerts for select to anon using (true);
