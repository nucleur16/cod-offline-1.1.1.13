"""Source-only profile projection; no compiler, DLL, WinAPI or process execution."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

def fault(text):
    start=text.index('static LONG CALLBACK FaultHandler(EXCEPTION_POINTERS *ep)')
    return text[start:text.index('\n/* ---- the Kernel32.dll stand-in',start)]

def project(body, diagnostic):
    result=[];enabled=True;stack=[]
    for line in body.splitlines(keepends=True):
        token=line.strip()
        if token.startswith('#if '):
            if token!='#if K33_DIAGNOSTICS':raise AssertionError('unexpected condition')
            stack.append(enabled);enabled=enabled and diagnostic
        elif token=='#endif':
            if not stack:raise AssertionError('unbalanced endif')
            enabled=stack.pop()
        elif token.startswith('#'):raise AssertionError('unexpected directive')
        elif enabled:result.append(line)
    if stack:raise AssertionError('unclosed condition')
    return ''.join(result)

def contract(body):
    if body.count('#if K33_DIAGNOSTICS')!=2:return False
    try:shipping=project(body,False);development=project(body,True)
    except AssertionError:return False
    if any(token in shipping for token in ('logged','InterlockedIncrement','VirtualQuery','GetModuleHandleExA','GetModuleFileNameA')):return False
    anchor='    if (InterlockedIncrement(&logged) <= 4) {'
    if development.count(anchor)!=1 or not shipping.endswith('    return EXCEPTION_CONTINUE_SEARCH;\n}\n'):return False
    if 'c->Eip = (DWORD)(DWORD_PTR)TakeOver;' not in shipping:return False
    if shipping.count('return EXCEPTION_CONTINUE_EXECUTION;')!=2:return False
    # Exactly the declaration and final diagnostic block may be omitted.
    prefix,diagnostic=development.split(anchor,1)
    prefix=prefix.replace('    static volatile LONG logged;\n','',1)
    return (shipping==prefix+'    return EXCEPTION_CONTINUE_SEARCH;\n}\n' and hashlib.sha256(' '.join(shipping.split()).encode()).hexdigest()==EXPECTED_FAULT_SHIPPING)

class FaultProfile(unittest.TestCase):
    def setUp(self):self.body=fault((ROOT/'src/kernel33.c').read_text(encoding='utf-8'))
    def test_shipping_has_no_diagnostic_memory_or_module_probes(self):self.assertTrue(contract(self.body))
    def test_diagnostic_profile_retains_counter_and_module_annotation(self):
        active=project(self.body,True)
        for token in ('InterlockedIncrement(&logged)','VirtualQuery(','GetModuleHandleExA(','GetModuleFileNameA('):self.assertIn(token,active)
    def test_shipping_preserves_functional_exception_dispatch(self):
        active=project(self.body,False)
        self.assertIn('c->Esp += 16;',active)
        self.assertIn('c->Esp = (c->Esp + 16) & ~0xFu;',active)
        self.assertIn('er->ExceptionInformation[0] == 8 && InTen(c->Eip) && Readable(sp, 16)',active)
    def test_exact_functional_return_state_and_dispatch_predicates_are_pinned(self):
        mutants=[self.body.replace('c->Eax = TRUE;','c->Eax = FALSE;',1),self.body.replace('er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION','er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION',1),self.body.replace('sp[1] == ten && sp[2] == DLL_PROCESS_ATTACH','sp[1] != ten && sp[2] == DLL_PROCESS_ATTACH',1),self.body.replace('sp[3] == ten','sp[3] != ten',1),self.body.replace('c->Esp += 16;','c->Esp += 12;',1),self.body.replace('(c->Esp + 16) & ~0xFu','(c->Esp + 12) & ~0xFu',1)]
        for i,body in enumerate(mutants):
            with self.subTest(mutant=i):self.assertFalse(contract(body))
    def test_hostile_profile_and_dispatch_mutations_are_rejected(self):
        mutants=[self.body.replace('#if K33_DIAGNOSTICS','#if 1',1),self.body.replace('#endif','',1),self.body.replace('#if K33_DIAGNOSTICS','',1),self.body.replace('GetModuleFileNameA(', 'VirtualQuery(',1).replace('#if K33_DIAGNOSTICS\n    if (InterlockedIncrement(&logged) <= 4) {','    if (InterlockedIncrement(&logged) <= 4) {',1),self.body.replace('c->Eip = (DWORD)(DWORD_PTR)TakeOver;','c->Eip = 0;',1),self.body.replace('return EXCEPTION_CONTINUE_EXECUTION;','return EXCEPTION_CONTINUE_SEARCH;',1)]
        for i,body in enumerate(mutants):
            with self.subTest(mutant=i):self.assertFalse(contract(body))



import hashlib
EXPECTED_FAULT_SHIPPING='adf398b8f0b5c24dc03315eadef60962f3484a79301ae9667d7548475a19659f'
EXPECTED_LOOPFX_SHIPPING='5922f27e5693f72935aa42e0068ff3f266c76e393e029309a3bab343fec5b93d'

def loopfx(text):
    start=text.index('static void __cdecl LoopFxProcessHook(int localClient)')
    return text[start:text.index('\nstatic void PatchLoopFxCatchUp(void)',start)]

def loop_contract(body):
    try:shipping=project(body,False)
    except AssertionError:return False
    if body.count('#if K33_DIAGNOSTICS')!=7:return False
    if any(token in shipping for token in ('logged','warned','moved','skipped','worst','InterlockedIncrement','Log(')):return False
    return hashlib.sha256(' '.join(shipping.split()).encode()).hexdigest()==EXPECTED_LOOPFX_SHIPPING

class LoopFxProfile(unittest.TestCase):
    def setUp(self):self.body=loopfx((ROOT/'src/kernel33.c').read_text(encoding='utf-8'))
    def test_shipping_retains_exact_catchup_and_original_call_without_statistics(self):self.assertTrue(loop_contract(self.body))
    def test_diagnostic_profile_retains_bounded_statistic_reporting(self):
        active=project(self.body,True)
        for token in ('skipped += steps;', 'moved++;', 'if (behind > worst) worst = behind;', 'InterlockedIncrement(&logged) <= 8', 'InterlockedIncrement(&warned) == 1'):self.assertIn(token,active)
    def test_hostile_guard_and_functional_drift_are_rejected(self):
        mutants=[self.body.replace('#if K33_DIAGNOSTICS','#if 1',1),self.body.replace('#endif','',1),self.body.replace('next[i] = (int)(next[i] + steps * period);','next[i] = 0;',1),self.body.replace('behind / period + 1','behind / period',1),self.body.replace(')(localClient);',')(0);',1),self.body.replace('period <= 0 || behind <= 0','period < 0 || behind <= 0',1)]
        for i,body in enumerate(mutants):
            with self.subTest(mutant=i):self.assertFalse(loop_contract(body))

if __name__=='__main__':unittest.main()
