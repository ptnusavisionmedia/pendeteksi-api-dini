// Helper bersama untuk fungsi Vercel (file berawalan _ tidak dijadikan endpoint)

function authorized(req) {
  const key = process.env.DEVICE_API_KEY;
  return key && req.headers['x-api-key'] === key;
}

function num(v) {
  return typeof v === 'number' && Number.isFinite(v) ? v : null;
}

async function insertRow(table, row) {
  const url = `${process.env.SUPABASE_URL}/rest/v1/${table}`;
  const r = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      apikey: process.env.SUPABASE_SERVICE_KEY,
      Authorization: `Bearer ${process.env.SUPABASE_SERVICE_KEY}`,
      Prefer: 'return=minimal',
    },
    body: JSON.stringify(row),
  });
  if (!r.ok) throw new Error(`Supabase ${table}: ${r.status} ${await r.text()}`);
}

module.exports = { authorized, num, insertRow };
