// Konfigurasi publik dashboard. Isi dengan nilai milikmu.
// ANON key Supabase memang publik (hanya bisa membaca karena RLS). JANGAN taruh service_role key,
// token Telegram, atau chat ID di sini.
window.APP_CONFIG = {
  MQTT_URL: 'wss://broker.hivemq.com:8884/mqtt',
  TOPIC_BASE: 'kampus/iot/kebakaran/',          // samakan dengan TOPIC_BASE di sketch.ino
  SUPABASE_URL: 'https://djnzzqxoxwxxaonwcprk.supabase.co',
  SUPABASE_ANON_KEY: 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImRqbnp6cXhveHd4eGFvbndjcHJrIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTQyMDE4NzYsImV4cCI6MjEwOTc3Nzg3Nn0.u6i7CE6BXxpX7uB6x_dHFoJ_AYhGozOX6Nc_BXg74eY',
  // Batas yang sama dengan sketch.ino (dipakai hanya untuk pewarnaan)
  TH: { temp: [45, 50], smoke: [40, 60], flame: [50, 70] },
};
