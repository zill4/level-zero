"""Generates Tools/levels/<id>.json theme files from Docs/levels_spec.json (the designer's spec) plus the greybox
layout knobs below (things the spec describes in prose). Plain Python, no Unreal needed:
    python Tools/levels_from_spec.py
Level 0 is kept as Tools/levels/level0.json (hand-written, matches the step3 build exactly); only its "next" is linked.
Colours: spec sRGB hex -> linear (what M_LZ_Surface expects). Distances: spec metres -> cm.
Fog: spec end distances past (StreamRadiusCells + 3) cells are clamped so streamed tiles never pop in visibly."""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = os.path.join(HERE, '..', 'Docs', 'levels_spec.json')
OUT = os.path.join(HERE, 'levels')


def lin(hexstr):
    h = hexstr.lstrip('#')
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(round(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4, 4))
    return out


MAPS = {'L1': 'L1_BulkAisles', 'L2': 'L2_Showroom', 'L3': 'L3_PoolRooms', 'L4': 'L4_Mall', 'L5': 'L5_Cinema', 'L6': 'L6_Suburb'}

# Greybox knobs per level (existing cube shapes + existing Tripo props only).
KNOBS = {
    'L1': dict(layout_style='aisles', cross_aisle_every=6, cross_aisle_width=2, partition_height=500, wall_thickness=90,
               room_size_min=[3, 3], room_size_max=[5, 4], loop_chance=0.04, stub_wall_chance=0.0,
               room_doorway_chance=0.0, corridor_doorway_chance=0.0, stream_radius_cells=6, light_fixture_scale=1.6,
               light_tint=[1.0, 0.98, 0.94], ambient=0.34, bounce=0.5, floor_light=0.7, ceil_light=0.5,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='LZ_WallVent', wall_vent_chance=0.02,
               clutter_wall_meshes=['LZ_Boxes'], clutter_free_meshes=['LZ_WetFloorSign'], num_clutter_clusters=26,
               cluster_wall_items=[1, 3], cluster_free_items=[0, 1], clusters_in_corridors=True,
               landmark_meshes=['LZ_VendingMachine'], landmarks_per_mesh=[2, 3], landmark_spacing=5, landmark_min_start_distance=8,
               min_openness=0.45,
               notes='Parallel aisle grid: 5 m rack walls (wall colour) on 4 m cells, 1-cell aisles, 2-cell cross-aisles every 6 cells, 9 m dark ceiling, high-bay fixtures (scaled light card) every 8 m. Rooms = pallet clearings. Boxes clusters line the racks.'),
    'L2': dict(partition_height=300, door_width=360, door_height=280, room_size_min=[3, 3], room_size_max=[4, 4],
               loop_chance=0.3, stub_wall_chance=0.0, room_doorway_chance=0.9, corridor_doorway_chance=0.0,
               alt_floor_pattern='rooms', light_fixture_scale=2.0, light_tint=[1.0, 0.88, 0.72],
               ambient=0.3, bounce=0.5, floor_light=0.55, ceil_light=0.25, prop_brightness=0.85,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='',
               clutter_wall_meshes=['LZ_FilingCabinet', 'LZ_Boxes'], clutter_free_meshes=['LZ_OfficeChair'], num_clutter_clusters=22,
               cluster_wall_items=[1, 2], cluster_free_items=[1, 2],
               landmark_meshes=['LZ_CRTCart'], landmarks_per_mesh=[3, 4], landmark_spacing=4, landmark_min_start_distance=6,
               min_openness=0.45,
               notes='Dark void hall: 3 m vignette partitions under a 12 m black ceiling, open-front rooms (3.6 m openings) with wood-tone floors (accent colour), dusty-rose carpet paths, sparse warm panels high up. Mezzanine/stairs not built (needs walkable verticality).'),
    'L3': dict(room_size_min=[3, 3], room_size_max=[5, 5], pillar_hall_size_min=[4, 4], pillar_hall_size_max=[6, 6], pillar_size=50,
               loop_chance=0.25, room_doorway_chance=0.4, corridor_doorway_chance=0.02,
               alt_floor_pattern='rooms', light_emissive=3.0, light_tint=[0.9, 0.96, 1.0],
               ambient=0.4, bounce=0.42, floor_light=0.6, ceil_light=0.72, fog_start_cm=1000,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='LZ_WallVent', wall_vent_chance=0.04,
               clutter_wall_meshes=[], clutter_free_meshes=['LZ_WetFloorSign'], num_clutter_clusters=10, cluster_free_items=[1, 2],
               landmark_meshes=[], min_openness=0.45,
               notes='Tiled 1-cell corridors, 4 m ceiling, cool light. Rooms are pools: opaque water-blue floor (alt floor) at deck level (no depth, no wading noise yet). Two tile-pillar forests. Sky-window corridor not built.'),
    'L4': dict(room_size_min=[2, 2], room_size_max=[3, 3], pillar_hall_size_min=[6, 6], pillar_hall_size_max=[6, 6], pillar_size=70,
               hall_ceiling_height=900, balcony_height=450, door_width=300, door_height=300,
               room_doorway_chance=0.8, corridor_doorway_chance=0.0, loop_chance=0.25,
               alt_floor_pattern='checker', light_tint=[1.0, 0.99, 0.96],
               ambient=0.45, bounce=0.55, floor_light=0.75, ceil_light=0.85,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='LZ_WallVent', wall_vent_chance=0.03,
               clutter_wall_meshes=['LZ_Boxes'], clutter_free_meshes=['LZ_WetFloorSign'], num_clutter_clusters=12,
               landmark_meshes=['LZ_VendingMachine', 'LZ_ArcadeCabinet'], landmarks_per_mesh=[2, 3], landmark_spacing=5,
               notes='2-cell concourses, 4.5 m ceiling, black/white checker floor, storefront rooms with 3 m openings, a 6x6 atrium (9 m) with a greybox upper balcony ring at 4.5 m (not walkable; no escalators/stairs yet).'),
    'L5': dict(room_size_min=[5, 7], room_size_max=[5, 7], room_ceiling_height=900, seat_tiers_in_rooms=True,
               room_doorway_chance=0.35, corridor_doorway_chance=0.02, loop_chance=0.2,
               alt_floor_pattern='stripes', light_emissive=2.5, light_tint=[1.0, 0.9, 0.9],
               ambient=0.3, bounce=0.55, floor_light=0.55, ceil_light=0.45,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='',
               clutter_wall_meshes=[], clutter_free_meshes=['LZ_WetFloorSign'], num_clutter_clusters=6, cluster_free_items=[1, 1],
               landmark_meshes=['LZ_ArcadeCabinet', 'LZ_VendingMachine'], landmarks_per_mesh=[1, 2], landmarks_in_corridors=True,
               landmark_min_start_distance=6,
               notes='Purple 2-cell hallways (3.5 m), striped carpet, 7 auditoriums (5x7 cells, 9 m ceilings) with visual stepped seat tiers (no collision, flat floor), sign-yellow sconces. No screens/curving spine yet.'),
    'L6': dict(width=20, height=20, partition_height=650, wall_thickness=300, room_size_min=[2, 2], room_size_max=[3, 3],
               loop_chance=0.3, stub_wall_chance=0.0, room_doorway_chance=0.0, corridor_doorway_chance=0.0,
               alt_floor_pattern='rooms', stream_radius_cells=4, light_fixture_scale=2.5, light_tint=[0.97, 0.99, 1.0],
               ambient=0.42, bounce=0.5, floor_light=0.7, ceil_light=0.8, fog_start_cm=600, fog_end_cm=3000,
               ceiling_light_mesh='LZ_CeilingLight', wall_vent_mesh='',
               clutter_wall_meshes=['LZ_Boxes', 'LZ_WaterCooler'], clutter_free_meshes=['LZ_OfficeChair'], num_clutter_clusters=10,
               landmark_meshes=[], min_openness=0.45,
               notes='8 m cells on a 20x20 grid under a 15 m drop ceiling; 6.5 m tall, 3 m thick "house row" blocks form 5 m streets; cul-de-sacs (rooms) have lawn floors. Aggressive fog (6-30 m, spec 15-70 m) and a 4-cell stream radius for the long sightlines.'),
}


def main():
    spec = json.load(open(SPEC))
    levels = {l['id']: l for l in spec['levels']}
    for lid, l in levels.items():
        if lid == 'L0':
            p = os.path.join(OUT, 'level0.json')
            t = json.load(open(p))
            t['next'] = 'L1'
            json.dump(t, open(p, 'w'), indent=2)
            continue
        k = dict(KNOBS[lid])
        cs = l['cell_size_m'] * 100
        radius = k.pop('stream_radius_cells', 5)
        fog_start = k.pop('fog_start_cm', l['fog']['start_m'] * 100)
        fog_end = k.pop('fog_end_cm', min(l['fog']['end_m'] * 100, (radius + 3) * cs))
        c = l['colors']
        t = {
            'level_id': lid, 'display_name': f"{lid} - {l['name']}", 'map': f"/Game/LevelZero/Maps/{MAPS[lid]}",
            'next': l['exit_to'] if l['exit_to'] in levels else '',
            'seed': 20261021 + l['order'],
            'width': 30, 'height': 30, 'cell_size': cs, 'corridor_width_cells': l['corridor_width_cells'],
            'ceiling_height': l['ceiling_height_m'] * 100, 'num_rooms': l['room_count'], 'num_pillar_halls': l['pillar_halls'],
            'stream_radius_cells': radius,
            'wall_color': lin(c['wall']), 'floor_color': lin(c['floor']), 'ceiling_color': lin(c['ceiling']),
            'trim_color': lin(c['accent']), 'light_color': lin(c['light_panel']),
            'alt_floor_color': lin(c.get('floor_alt', c['accent'])),
            'fog_color': lin(l['fog']['color']), 'fog_start': fog_start, 'fog_end': fog_end,
            'light_spacing_cells': max(1, round(l['light_panel_spacing_m'] * 100 / cs)),
            'all_light_cells_lit': False, 'lit_panel_chance': l['lit_panel_chance'], 'dead_panel_chance': l['dead_panel_chance'],
            'spec': {'name': l['name'], 'refs': l['refs'], 'fog_spec_m': [l['fog']['start_m'], l['fog']['end_m']],
                     'monster_twist': l['monster_twist'], 'transition': l['transition'], 'new_props_needed': l['new_props_needed']},
        }
        t.update(k)
        json.dump(t, open(os.path.join(OUT, f'{lid}.json'), 'w'), indent=2)
        print(lid, MAPS[lid], 'fog', fog_start, fog_end, 'lights every', t['light_spacing_cells'])


main()
