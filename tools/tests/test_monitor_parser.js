// test_monitor_parser.js
// Tests the stream parser of tools/Eaglehagen_Serial_Monitor.html.
// Run from anywhere with Node.js:  node tools/tests/test_monitor_parser.js
// capture_labview.bin is ~4 s of real device output (LabVIEW format, starts mid-stream).

const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'Eaglehagen_Serial_Monitor.html'), 'utf8');
const code = html.split('// ==== PARSER START ====')[1].split('// ==== PARSER END ====')[0];
const { StreamParser, EtTracker, MMHG_TO_KPA } =
  new Function(code + '; return { StreamParser, EtTracker, MMHG_TO_KPA };')();

let fail = 0;
const check = (name, cond, extra) => {
  console.log((cond ? 'PASS ' : 'FAIL ') + name + (extra ? '  ' + extra : ''));
  if (!cond) fail++;
};
function run(bytes, mode, chunk) {
  const frames = [], texts = [];
  const p = new StreamParser(f => frames.push(f), t => texts.push(t));
  p.mode = mode || 'auto';
  for (let i = 0; i < bytes.length; i += chunk) p.push(bytes.subarray(i, i + chunk));
  return { frames, texts, errors: p.errors };
}
const enc = s => Uint8Array.from(Buffer.from(s, 'latin1'));

// 1. Real capture from the device, fed in odd-sized chunks
const cap = new Uint8Array(fs.readFileSync(path.join(__dirname, 'capture_labview.bin')));
const nFrames = Math.floor(cap.length / 24);
const r1 = run(cap, 'auto', 13);
check('real capture: all frames parsed', r1.frames.length === nFrames, `${r1.frames.length}/${nFrames} frames, ${r1.errors} errors`);
const f = r1.frames[0];
check('real capture: plausible values', f.format === 'labview' && f.s1 === 6 && f.o2 > 15 && f.o2 < 25 && f.volUnit === 'ADC', JSON.stringify(f));

// 2. Joining mid-frame resyncs on the next frame
const r2 = run(cap.subarray(10), 'auto', 5);
check('mid-frame start: resyncs', r2.frames.length === nFrames - 1, `${r2.frames.length} frames`);

// 3. Binary tail bytes that look like markers: EtCO2 = 27 mmHg (0x1B), RR = 10 (0x0A)
const r3 = run(enc('\x1b035\t00209\t00330\t\x06\x80\x0a\xff\x1b\r\n'), 'auto', 3);
check('binary tail with 0x1B / 0x0A', r3.frames.length === 1 && r3.frames[0].rr === 10 &&
  Math.abs(r3.frames[0].et - 27 * MMHG_TO_KPA) < 1e-9 && Math.abs(r3.frames[0].co2 - 35 * MMHG_TO_KPA) < 1e-9);

// 4. ASCII format with '#' diagnostics, CRLF and a blank line
const ascii = enc('# Host output format: Tab-separated ASCII\r\n4.3\t20.9\t14\t320\t6\t0\r\n\r\n0.0\t20.8\t14\t310\t6\t1\r\n# # Checksum error\r\n');
const r4 = run(ascii, 'auto', 4);
check('ASCII frames', r4.frames.length === 2 && r4.frames[0].co2 === 4.3 && r4.frames[0].volUnit === 'mL' && r4.frames[1].s2 === 1);
check('ASCII # lines go to the log', r4.texts.length === 2 && r4.texts.every(t => t.startsWith('#')));

// 5. BOOT pressed while connected: LabVIEW frames, then ASCII lines
const r5 = run(new Uint8Array([...cap.subarray(0, 48), ...ascii]), 'auto', 9);
check('format switch mid-stream', r5.frames.map(x => x.format).join(',') === 'labview,labview,ascii,ascii');

// 6. Forced LabVIEW mode ignores ASCII data lines
check('forced LabVIEW mode ignores ASCII', run(ascii, 'labview', 4).frames.length === 0);

// 7. Garbage before valid frames
const junk = new Uint8Array(5000).map((_, i) => (i * 37 + 11) & 0xff);
const r7 = run(new Uint8Array([...junk, ...cap]), 'auto', 64);
check('garbage then frames', r7.frames.length >= nFrames - 1, `${r7.frames.length} frames`);

// 8. EtCO2 tracker (used for the ASCII format) on a synthetic capnogram in kPa
const et = new EtTracker(); let last = 0;
for (let b = 0; b < 3; b++) for (let k = 0; k < 40; k++) { const ph = k / 40; last = et.push(ph > 0.4 && ph < 0.9 ? 5.3 : 0); }
check('EtCO2 tracker', Math.abs(last - 5.3) < 1e-9, 'et=' + last);

console.log(fail ? `${fail} FAILED` : 'ALL PASSED');
process.exit(fail ? 1 : 0);
