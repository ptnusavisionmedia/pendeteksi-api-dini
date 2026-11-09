// Konfigurasi publik dashboard. Isi dengan nilai milikmu.
// ANON key Supabase memang publik (hanya bisa membaca karena RLS). JANGAN taruh service_role key,
// token Telegram, atau chat ID di sini.
window.APP_CONFIG = {
  MQTT_URL: 'wss://broker.hivemq.com:8884/mqtt',
  TOPIC_BASE: 'kampus/iot/kebakaran/',          // samakan dengan TOPIC_BASE di sketch.ino
  SUPABASE_URL: 'https://ISI_PROJECT_ID.supabase.co',
  SUPABASE_ANON_KEY: 'ISI_ANON_KEY',
  // Batas yang sama dengan sketch.ino (dipakai hanya untuk pewarnaan)
  TH: { temp: [45, 50], smoke: [40, 60], flame: [50, 70] },
};
