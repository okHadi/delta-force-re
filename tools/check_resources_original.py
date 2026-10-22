#!/usr/bin/env python3
"""Check the separate 4633F0 load pass and assembled stock model handles."""
import argparse
import json
import subprocess
from pathlib import Path
import pefile
from check_resource_plan_original import Planner, read_items, read_missions, read_weapon_table, item_field, TYPE_NUMBERS
from check_weapons_original import OriginalRig, ITEM_TABLE
from unicorn.x86_const import UC_X86_REG_ESP
from pff import Archive

MODEL_BASE = 0x71000000


class Rig(OriginalRig):
    def __init__(self, game, items):
        super().__init__(game)
        self.items = items
        self.stubs.update({0x4633C0: self.stub_void, 0x4633E0: self.stub_void,
                           0x486530: self.model_load, 0x45D6F0: self.item_enter})

    def args(self, count):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        return [self.peek(sp + 4 + i * 4) for i in range(count)]

    def item_enter(self):
        self.loads.append(self.args(1)[0])
        # Execute the real 45D6F0 body.

    def model_load(self):
        name, bank, fresh = self.args(3)
        name = bytes(self.u.mem_read(name, 16)).split(b'\0')[0].decode('latin1').lower() + '.3di'
        self.poke(fresh, 0)
        if name not in self.present:
            self.finish(0)
            return
        if name not in self.registry:
            self.registry[name] = (MODEL_BASE + len(self.registry), bank)
        self.finish(self.registry[name][0])

    def run(self, prior, present):
        self.reset()
        self.loads, self.registry, self.present = [], {}, present
        self.poke(0xC23810, ITEM_TABLE)
        self.poke(0xC23814, len(self.items))
        self.poke(0x503138, 1)
        self.u.mem_write(ITEM_TABLE, bytes(len(self.items) * 0x13C))
        for i, item in enumerate(self.items):
            record = ITEM_TABLE + i * 0x13C
            self.poke(record + 0x20, item['rid'])
            self.poke(record + 0x28, TYPE_NUMBERS.get(item['type'], 0))
            for offset, key in [(0x2C, 'graphic'), (0x3C, 'husk')]:
                self.u.mem_write(record + offset, item_field(item, key).encode('latin1') + b'\0')
        for step in prior.steps:
            if step['kind'] not in ['model', 'husk']:
                continue
            name = step['name'].lower() + '.3di'
            if name not in present:
                continue
            if name not in self.registry:
                self.registry[name] = (MODEL_BASE + len(self.registry), step['bank'])
            self.poke(ITEM_TABLE + step['index'] * 0x13C + (0x6C if step['kind'] == 'model' else 0x70), self.registry[name][0])
        before = len(self.registry)
        self.call(0x4633F0)
        result = []
        for index in self.loads:
            item = self.items[index]
            handle = self.peek(ITEM_TABLE + index * 0x13C + 0x6C)
            name = item_field(item, 'graphic').lower() + '.3di'
            result.append({'id': item['rid'], 'index': index, 'handle': handle,
                           'name': name if handle else '', 'bank': self.registry[name][1] if handle else 0})
        assert [self.items[i]['rid'] for i in self.loads] == [0xFA1, 0xFC1, 0xFCA]
        for index, address in zip(self.loads, [0xA158B4, 0xA158B8, 0xA158B0]):
            assert self.peek(address) == index
        return before, len(self.registry), result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    pe = pefile.PE(str(args.game / 'Df.exe'))
    items = read_items(args.game, pe)
    weapons = read_weapon_table((args.game / 'Df.exe').read_bytes(), pe)
    missions = read_missions(args.game)
    present = {entry.name.lower() for name in ['Dfbase.pff', 'Df.pff']
               for entry in Archive(args.game / name).entries if not entry.deleted}
    options = {'color32': True, 'multiplayer': False, 'squad': False, 'powerup': True, 'patch': True}
    rig = Rig(args.game, items)
    rows = subprocess.check_output([str(args.probe.resolve()), str(args.game), '--projectiles'], text=True).splitlines()
    for row in rows:
        actual = json.loads(row)
        mission = missions[actual['mission']]
        prior = Planner(items, weapons, options)
        prior.soldiers(mission['theme'], mission['night'])
        prior.weapon_stage()
        prior.effect_stage()
        prior.mission_stages(mission['groups'])
        before, after, loads = rig.run(prior, present)
        expected = {'mission': actual['mission'], 'before_models': before, 'models': after, 'loads': loads}
        if actual != expected:
            raise AssertionError(f'original={expected}\nnative={actual}')
    assert len(rows) == len(missions)
    _, _, missing = rig.run(prior, present - {'m406.3di', 'law_rckt.3di'})
    assert missing[0]['handle'] == 0 and missing[2]['handle'] == 0
    # Original SP caller has adjacent 402EC0 then 4633F0 calls.
    code = pe.get_memory_mapped_image()
    assert code[0x3C12:0x3C1C] == bytes.fromhex('e8a9f2ffffe8d4f70500')
    report = {'status': 'passed', 'missions': len(rows), 'item_loads': len(rows) * 3,
              'source_sha256': 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff',
              'real_bodies': '4633F0,4828C0,45D6F0; file registry providers; actual stock assembly',
              'caller_order': '403C12:402EC0,403C17:4633F0',
              'missing_model_fixture': '4633F0 retains null models for missing m406.3di and law_rckt.3di'}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
