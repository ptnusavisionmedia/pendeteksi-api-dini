const { authorized, num, insertRow } = require('./_lib');

// POST /api/alert  - kirim notifikasi Telegram + catat ke Supabase.
// Token bot dan chat ID hanya ada di Environment Variable Vercel.
module.exports = async (req, res) => {
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });
  if (!authorized(req)) return res.status(401).json({ error: 'unauthorized' });

  const b = req.body || {};
  const device = String(b.device || 'esp32-01').slice(0, 40);
  const t = num(b.temperature), s = num(b.smoke), f = num(b.flame);
  const status = String(b.status || 'bahaya').slice(0, 20);

  const text =
    '🔥 POTENSI KEBAKARAN TERDETEKSI\n' +
    `Perangkat: ${device}\n` +
    `Suhu: ${t ?? '-'} °C\n` +
    `Asap (gas): ${s ?? '-'}%\n` +
    `Api (LDR): ${f ?? '-'}%\n` +
    `Status: ${status}`;

  let telegramOk = false;
  try {
    const r = await fetch(`https://api.telegram.org/bot${process.env.TELEGRAM_BOT_TOKEN}/sendMessage`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ chat_id: process.env.TELEGRAM_CHAT_ID, text }),
    });
    telegramOk = r.ok;
    if (!r.ok) console.error('Telegram gagal:', r.status);
  } catch (e) {
    console.error('Telegram error:', e.message);
  }

  try {
    await insertRow('alerts', { device, temperature: t, smoke: s, flame: f, status, telegram_ok: telegramOk });
  } catch (e) {
    console.error(e.message);
  }

  return res.status(200).json({ ok: true, telegram: telegramOk });
};
