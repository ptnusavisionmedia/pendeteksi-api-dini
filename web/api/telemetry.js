const { authorized, num, insertRow } = require('./_lib');

// POST /api/telemetry  - menyimpan riwayat pembacaan sensor ke Supabase
module.exports = async (req, res) => {
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });
  if (!authorized(req)) return res.status(401).json({ error: 'unauthorized' });

  const b = req.body || {};
  try {
    await insertRow('readings', {
      device: String(b.device || 'esp32-01').slice(0, 40),
      temperature: num(b.temperature),
      humidity: num(b.humidity),
      smoke: num(b.smoke),
      flame: num(b.flame),
      status: String(b.status || '').slice(0, 20),
      ts_ms: num(b.ts_ms),
    });
    return res.status(200).json({ ok: true });
  } catch (e) {
    console.error(e.message);
    return res.status(500).json({ error: 'db error' });
  }
};
