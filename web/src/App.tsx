import { useState, useEffect } from 'react';
import { 
  Bluetooth, 
  BluetoothConnected, 
  Volume2, 
  RotateCw, 
  Mic, 
  Power, 
  Sparkles,
  Clock,
  CheckCircle2,
  AlertCircle
} from 'lucide-react';
import { bleService } from './ble';
import type { DeviceConfig } from './ble';

export default function App() {
  const [isConnected, setIsConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [successMsg, setSuccessMsg] = useState<string | null>(null);

  // Device settings
  const [speed, setSpeed] = useState<number>(3);
  const [threshold, setThreshold] = useState<number>(80);
  const [volume, setVolume] = useState<number>(20);
  const [isRunning, setIsRunning] = useState<boolean>(false);

  useEffect(() => {
    bleService.onDisconnectCallback = () => {
      setIsConnected(false);
      setSuccessMsg(null);
      setErrorMsg('การเชื่อมต่อบลูทูธสิ้นสุดลง');
    };

    bleService.onStateChangeCallback = (state) => {
      setIsRunning(state);
    };
  }, []);

  const handleConnect = async () => {
    try {
      setConnecting(true);
      setErrorMsg(null);
      setSuccessMsg(null);
      const config: DeviceConfig = await bleService.connect();
      setSpeed(config.speed);
      setThreshold(config.threshold);
      setVolume(config.volume);
      setIsRunning(config.isRunning);
      setIsConnected(true);
      setSuccessMsg('เชื่อมต่อสำเร็จ! ดึงข้อมูลจาก ESP32 เรียบร้อย');
    } catch (err: unknown) {
      const e = err as Error;
      if (e.name !== 'NotFoundError') { // Don't show error if user just cancelled device picker
        setErrorMsg(e.message || 'ไม่สามารถเชื่อมต่อบลูทูธได้');
      }
    } finally {
      setConnecting(false);
    }
  };

  const handleDisconnect = () => {
    bleService.disconnect();
    setIsConnected(false);
  };

  const handleSpeedChange = async (newSpeed: number) => {
    setSpeed(newSpeed);
    try {
      await bleService.writeSpeed(newSpeed);
    } catch (e) {
      console.error(e);
    }
  };

  const handleThresholdChange = async (newThresh: number) => {
    setThreshold(newThresh);
    try {
      await bleService.writeThreshold(newThresh);
    } catch (e) {
      console.error(e);
    }
  };

  const handleVolumeChange = async (newVol: number) => {
    setVolume(newVol);
    try {
      await bleService.writeVolume(newVol);
    } catch (e) {
      console.error(e);
    }
  };

  const handleToggleState = async () => {
    const nextState = !isRunning;
    setIsRunning(nextState);
    try {
      await bleService.writeState(nextState);
    } catch (e) {
      console.error(e);
      setIsRunning(!nextState); // Rollback on error
    }
  };

  // 4096 steps per rev: 4096 * delay_ms / 1000 = seconds per rev
  const secPerRev = ((4096 * speed) / 1000).toFixed(1);

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-950 via-slate-900 to-indigo-950 text-slate-100 flex flex-col items-center p-4 sm:p-6">
      {/* Header */}
      <header className="w-full max-w-md text-center py-6">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-indigo-500/10 border border-indigo-500/20 text-indigo-400 text-xs font-medium mb-3">
          <Sparkles className="w-3.5 h-3.5 text-amber-400" />
          <span>33rd Anniversary Interactive Gift</span>
        </div>
        <h1 className="text-2xl sm:text-33xl font-bold tracking-tight bg-gradient-to-r from-white via-indigo-100 to-indigo-300 bg-clip-text text-transparent">
          กล่องดนตรี 33 ปี
        </h1>
        <p className="text-xs sm:text-sm text-slate-400 mt-1">
          ตัวควบคุม & ปรับแต่งค่าผ่านระบบ Web Bluetooth
        </p>
      </header>

      {/* Main Container */}
      <main className="w-full max-w-md space-y-4">
        {/* Connect Card */}
        <div className="bg-slate-900/80 backdrop-blur-xl border border-slate-800 rounded-2xl p-5 shadow-2xl">
          <div className="flex items-center justify-between mb-4">
            <div className="flex items-center gap-3">
              <div className={`w-10 h-10 rounded-xl flex items-center justify-center ${isConnected ? 'bg-emerald-500/20 text-emerald-400' : 'bg-slate-800 text-slate-400'}`}>
                {isConnected ? <BluetoothConnected className="w-5 h-5 animate-pulse" /> : <Bluetooth className="w-5 h-5" />}
              </div>
              <div>
                <h2 className="text-sm font-semibold text-white">สถานะการเชื่อมต่อ</h2>
                <div className="flex items-center gap-1.5 mt-0.5">
                  <span className={`w-2 h-2 rounded-full ${isConnected ? 'bg-emerald-400 animate-ping' : 'bg-slate-500'}`} />
                  <span className={`text-xs ${isConnected ? 'text-emerald-400 font-medium' : 'text-slate-400'}`}>
                    {isConnected ? 'เชื่อมต่อ ESP32 แล้ว' : 'ยังไม่ได้เชื่อมต่อ'}
                  </span>
                </div>
              </div>
            </div>

            {isConnected ? (
              <button
                onClick={handleDisconnect}
                className="px-3.5 py-1.5 rounded-xl text-xs font-medium bg-rose-500/10 text-rose-400 border border-rose-500/20 hover:bg-rose-500/20 transition-all cursor-pointer"
              >
                ตัดการเชื่อมต่อ
              </button>
            ) : (
              <button
                onClick={handleConnect}
                disabled={connecting}
                className="px-4 py-2 rounded-xl text-xs font-semibold bg-indigo-600 hover:bg-indigo-500 text-white shadow-lg shadow-indigo-600/30 transition-all flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
              >
                {connecting ? (
                  <>
                    <span className="w-3.5 h-3.5 border-2 border-white/20 border-t-white rounded-full animate-spin" />
                    <span>กำลังค้นหา...</span>
                  </>
                ) : (
                  <>
                    <Bluetooth className="w-4 h-4" />
                    <span>เชื่อมต่อบลูทูธ</span>
                  </>
                )}
              </button>
            )}
          </div>

          {/* Feedback messages */}
          {errorMsg && (
            <div className="flex items-center gap-2 p-3 rounded-xl bg-rose-500/10 border border-rose-500/20 text-rose-300 text-xs mt-3">
              <AlertCircle className="w-4 h-4 shrink-0 text-rose-400" />
              <span>{errorMsg}</span>
            </div>
          )}
          {successMsg && (
            <div className="flex items-center gap-2 p-3 rounded-xl bg-emerald-500/10 border border-emerald-500/20 text-emerald-300 text-xs mt-3">
              <CheckCircle2 className="w-4 h-4 shrink-0 text-emerald-400" />
              <span>{successMsg}</span>
            </div>
          )}

          {/* Tip */}
          <div className="mt-3 pt-3 border-t border-slate-800/80 flex items-start gap-2 text-[11px] text-slate-400">
            <Clock className="w-3.5 h-3.5 shrink-0 text-indigo-400 mt-0.5" />
            <span>
              <strong>Tip:</strong> บลูทูธจะเปิดให้เชื่อมต่อเฉพาะช่วง <strong>5 นาทีแรกหลังเสียบปลั๊ก</strong> (ถ้าไม่พบอุปกรณ์ ให้ถอดปลั๊กเสียบใหม่)
            </span>
          </div>
        </div>

        {/* Remote Control / Manual Trigger */}
        <div className={`bg-slate-900/80 backdrop-blur-xl border border-slate-800 rounded-2xl p-5 shadow-xl transition-opacity duration-300 ${!isConnected ? 'opacity-50 pointer-events-none' : ''}`}>
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className={`w-10 h-10 rounded-xl flex items-center justify-center transition-colors ${isRunning ? 'bg-amber-500/20 text-amber-400' : 'bg-slate-800 text-slate-400'}`}>
                <Power className="w-5 h-5" />
              </div>
              <div>
                <h3 className="text-sm font-semibold text-white">ทดสอบสั่งงาน (Manual Trigger)</h3>
                <p className="text-xs text-slate-400">
                  {isRunning ? 'กำลังทำงาน (มอเตอร์หมุน + ไฟติด)' : 'เครื่องหยุดนิ่ง (สแตนด์บาย)'}
                </p>
              </div>
            </div>

            <button
              onClick={handleToggleState}
              className={`px-5 py-2.5 rounded-xl text-xs font-bold transition-all shadow-md cursor-pointer ${
                isRunning 
                  ? 'bg-rose-600 hover:bg-rose-500 text-white shadow-rose-600/30' 
                  : 'bg-emerald-600 hover:bg-emerald-500 text-white shadow-emerald-600/30'
              }`}
            >
              {isRunning ? 'สั่งปิด (กลับ 0°)' : 'สั่งเปิดทำงาน'}
            </button>
          </div>
        </div>

        {/* Settings Sliders */}
        <div className={`space-y-4 bg-slate-900/80 backdrop-blur-xl border border-slate-800 rounded-2xl p-5 shadow-xl transition-opacity duration-300 ${!isConnected ? 'opacity-50 pointer-events-none' : ''}`}>
          <div className="flex items-center justify-between pb-3 border-b border-slate-800">
            <h3 className="text-sm font-semibold text-white">ปรับแต่งค่าพารามิเตอร์</h3>
            <span className="text-[11px] text-slate-400">บันทึกอัตโนมัติลงชิป</span>
          </div>

          {/* Stepper Speed */}
          <div className="space-y-2">
            <div className="flex items-center justify-between text-xs">
              <span className="text-slate-300 font-medium flex items-center gap-2">
                <RotateCw className="w-4 h-4 text-indigo-400" />
                ความเร็วมอเตอร์หมุนโมเดล
              </span>
              <span className="text-indigo-300 font-mono font-semibold">
                {speed} ms (~{secPerRev} วินาที/รอบ)
              </span>
            </div>
            <input
              type="range"
              min="1"
              max="8"
              step="1"
              value={speed}
              onChange={(e) => handleSpeedChange(Number(e.target.value))}
              className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-500"
            />
            <div className="flex justify-between text-[10px] text-slate-500">
              <span>หมุนเร็ว (1ms ~4วิ)</span>
              <span>ปกติ (3ms ~12วิ)</span>
              <span>หมุนช้าเนิบๆ (8ms ~32วิ)</span>
            </div>
          </div>

          {/* Wake Word Threshold */}
          <div className="space-y-2 pt-2">
            <div className="flex items-center justify-between text-xs">
              <span className="text-slate-300 font-medium flex items-center gap-2">
                <Mic className="w-4 h-4 text-violet-400" />
                ความไวตรวจจับคำพูด ("สามสิบสามปี")
              </span>
              <span className="text-violet-300 font-mono font-semibold">
                {(threshold / 100).toFixed(2)} ({threshold}%)
              </span>
            </div>
            <input
              type="range"
              min="50"
              max="95"
              step="1"
              value={threshold}
              onChange={(e) => handleThresholdChange(Number(e.target.value))}
              className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-violet-500"
            />
            <div className="flex justify-between text-[10px] text-slate-500">
              <span>ติดง่ายมาก (0.50)</span>
              <span>แนะนำ (0.80)</span>
              <span>ต้องพูดชัดเป๊ะ (0.95)</span>
            </div>
          </div>

          {/* DFPlayer Volume */}
          <div className="space-y-2 pt-2">
            <div className="flex items-center justify-between text-xs">
              <span className="text-slate-300 font-medium flex items-center gap-2">
                <Volume2 className="w-4 h-4 text-amber-400" />
                ระดับเสียงเพลง (DFPlayer Mini)
              </span>
              <span className="text-amber-300 font-mono font-semibold">
                {volume} / 30
              </span>
            </div>
            <input
              type="range"
              min="0"
              max="30"
              step="1"
              value={volume}
              onChange={(e) => handleVolumeChange(Number(e.target.value))}
              className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-amber-500"
            />
            <div className="flex justify-between text-[10px] text-slate-500">
              <span>ปิดเสียง (0)</span>
              <span>ปานกลาง (15-20)</span>
              <span>ดังสุด (30)</span>
            </div>
          </div>
        </div>

        {/* Footer info */}
        <footer className="text-center py-4 text-slate-500 text-xs space-y-1">
          <p>ESP32-S3 BLE Service • Edge Impulse Wake Word</p>
          <p className="text-[11px] text-slate-600">รองรับ Google Chrome & Microsoft Edge บน Android / PC / Mac</p>
        </footer>
      </main>
    </div>
  );
}
