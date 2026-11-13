import pathlib,re,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=(ROOT/'src/console/console.c').read_text()
def body(name):
    start=SOURCE.index('\n{',SOURCE.index(name))+2
    depth=1;pos=start
    while depth:
        pos+=1
        if SOURCE[pos]=='{':depth+=1
        elif SOURCE[pos]=='}':depth-=1
    return SOURCE[start:pos]
class ConsoleWorker(unittest.TestCase):
    def test_present_consumes_only_completed_cpu_frames(self):
        code=body('static void ConDraw(')
        for forbidden in ('ConRender(', 'ConTakeSnapshot(', 'ConEnsureDib(', 'fopen(', 'fwrite(',
                          'WaitFor', 'Sleep(', 'EnterCriticalSection(', 'GdiFlush(', 'CreateFont'):
            self.assertNotIn(forbidden,code)
        self.assertIn('ConFrameRead(frame)',code)
        self.assertIn('frame->serial - g_conUploadedSerial',code)
        self.assertIn('frame->w == W && frame->h == H',code)
    def test_worker_owns_raster_and_io_not_d3d_or_game_pointer_reads(self):
        code=body('static DWORD WINAPI ConRasterWorker(void *unused)\n{')
        for expected in ('ConLoadHistory(', 'ConSaveHistory(', 'ConTakeSnapshot(', 'ConRender(', 'ConSaveShot('):
            self.assertIn(expected,code)
        for forbidden in ('IDirect3D', 'ConSnapshotDvars(', 'C1Dvar', 'ConUpdateSuggestions('):
            self.assertNotIn(forbidden,code)
        self.assertIn('16777216',code)
    def test_window_edit_history_never_performs_disk_io(self):
        for name in ('static void ConAddHistory(', 'static void ConSetOpen('):
            code=body(name)
            self.assertNotIn('fopen(',code)
            self.assertNotIn('ConLoadHistory(',code)
    def test_empty_prompt_avoids_live_dvar_traversal(self):
        code=body('static void ConUpdateSuggestions(')
        self.assertLess(code.index('if (!g_conInputLen)'),code.index('dcount = ConSnapshotDvars()'))
    def test_snapshot_is_bounded_and_does_not_wait_on_input_or_log(self):
        code=body('static BOOL ConTakeSnapshot(')
        self.assertIn('TryEnterCriticalSection(&g_conUiLock)',code)
        self.assertIn('TryEnterCriticalSection(&g_conLock)',code)
        self.assertNotIn('ConDvar',code)
        self.assertNotIn('fopen(',code)
if __name__=='__main__':unittest.main()
