"""Static cutover contract; no compiler, process, app, or DLL execution."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(text, name):
    start = text.index(name)
    begin = text.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def profile_gate(text):
    start = text.index('    /* Full migration must not enter C1')
    end = text.index('    Log("entering game', start)
    body = text[start:end].split('#if CODOL_KERNEL_NAME_DISPLAY\n', 1)[1]
    full, default = body.split('#else\n', 1)
    return full, default.split('#endif', 1)[0]


def required(text):
    full, _ = profile_gate(text)
    return ('if (!K33HasMainTick(TheatreTick) || !K33InstallMainTicks())' in full
            and 'Fatal("Kernel33 main-thread migration checks failed.' in full)


def checked(text):
    body = function(text, 'static BOOL K33InstallMainTicks(void)')
    return ('if (!CallsTarget(' in body
            and body.index('RedirectCall(') < body.index('if (!CallsTarget(')
            < body.index('return TRUE;'))


def room_profile(body, room, local):
    out=[];enabled=True;stack=[]
    for line in body.splitlines(True):
        token=line.strip()
        if token.startswith('#if'):
            if token!='#if CODOL_KERNEL_ROOM_CONTEXT':raise AssertionError('unexpected target condition')
            stack.append(enabled);enabled=enabled and room
        elif token=='#endif':
            if not stack:raise AssertionError('unbalanced target condition')
            enabled=stack.pop()
        elif token.startswith('#'):raise AssertionError('unexpected target directive')
        elif enabled:out.append(line)
    if stack:raise AssertionError('unclosed target condition')
    return ''.join(out)


class CutoverContract(unittest.TestCase):
    def setUp(self):
        self.kernel = (ROOT / 'src/kernel33.c').read_text(encoding='utf8')
        self.tick = (ROOT / 'src/k33tick.c').read_text(encoding='utf8')

    def test_full_profile_requires_callback_and_dispatcher(self):
        self.assertTrue(required(self.kernel))
        for before in ('!K33HasMainTick(TheatreTick) || ', ' || !K33InstallMainTicks()'):
            self.assertFalse(required(self.kernel.replace(before, '', 1)))

    def test_default_profile_and_dependency_remain(self):
        _, default = profile_gate(self.kernel)
        self.assertEqual(default.strip(), 'K33InstallMainTicks(); /* last: chains in front of the retained client tick hooks */')
        profile = (ROOT / 'src/production_profile.h').read_text(encoding='utf8')
        self.assertIn('#define CODOL_KERNEL_ROOM_CONTEXT 0', profile)
        self.assertIn('#define CODOL_KERNEL_NAME_DISPLAY 0', profile)
        self.assertIn('#if CODOL_KERNEL_NAME_DISPLAY && !CODOL_KERNEL_ROOM_CONTEXT', profile)

    def test_success_follows_exact_readback(self):
        self.assertTrue(checked(self.tick))
        self.assertFalse(checked(self.tick.replace('if (!CallsTarget(', 'if (0 && !CallsTarget(', 1)))

    def test_whitelist_is_closed_and_unchanged(self):
        body = function(self.tick, 'static BOOL K33InstallMainTicks(void)')
        for room,local in ((False,False),(True,False),(False,True),(True,True)):
            active=room_profile(body,room,local)
            self.assertEqual(active.count('target !='), 4+int(room))
            for name in ('VOICE_EVENTLOOP_FUNC_VA', 'VOICE_TickHook', 'DISCORD_TickHook', 'PI_TickHook'):
                self.assertIn(name, active)
            self.assertEqual('KRC_MainTickHook' in active, room)
        with self.assertRaises(AssertionError):room_profile(body.replace('#if CODOL_KERNEL_ROOM_CONTEXT','#if 1'),True,False)

    def test_membership_uses_actual_callback_list(self):
        body = function(self.tick, 'static BOOL K33HasMainTick(K33TickFn fn)')
        self.assertIn('i < g_k33TickCount', body)
        self.assertIn('g_k33Ticks[i] == fn', body)
        self.assertIn('return FALSE;', body)

    def test_hook_order_and_renderer_boundary_remain(self):
        start = self.kernel.index('    InstallVoice();')
        tail = self.kernel[start:self.kernel.index('    Log("entering game', start)]
        self.assertLess(tail.index('InstallKernelRoomContext()'), tail.index('InstallP2PInprocDiagnostics()'))
        self.assertLess(tail.index('InstallP2PInprocDiagnostics()'), tail.index('InstallP2PHost()'))
        self.assertLess(tail.index('InstallP2PHost()'), tail.index('K33HasMainTick(TheatreTick)'))
        self.assertNotIn('InstallP2PInproc();',tail)
        host=(ROOT/'src/p2p/p2p.c').read_text();start=function(host,'static BOOL P2PStartConfigured(')
        self.assertLess(start.index('P2PEnsureInprocActivation()'),start.index('CreateThread('))
        pi=(ROOT/'src/p2p/p2p_inproc.c').read_text();activation=function(pi,'static int PiActivationTick(')
        self.assertIn('K33OnMainTick(PiTick)',activation)
        self.assertNotIn('RedirectCall(',activation)
        self.assertIn('g_k33Ticks[i]!=w->ticks[i]',function(pi,'static int PiActivationChain('))
        self.assertNotIn('Direct3DCreate9', function(self.tick, 'static BOOL K33InstallMainTicks(void)'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
