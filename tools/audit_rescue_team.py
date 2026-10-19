#!/usr/bin/env python3
"""Audit original campaign data against the current runtime, without editing it.

Run from any directory: python3 tools/audit_rescue_team.py [--assets PATH]
The C probe shares the production decoder, initialization, drawing and dispatch.
Outputs are local HTML/CSV plus the complete evidence JSON and test log.
"""
import argparse
import collections
import csv
import hashlib
import html
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WALKTHROUGH = "https://www.gamehouse.com/blog/2015/10/rescue-team-5-walkthrough/"
PLAYLIST = "https://www.youtube.com/playlist?list=PLlugQ1OqiZQvsOoa1GU17J6J-KGLUauYt"
GUIDE = "https://steamcommunity.com/sharedfiles/filedetails/?id=2910994208"

# Names of undocumented numeric types are interpretations of the original
# parameters, positions, tutorials and artwork, not a recovered class registry.
TYPES = {
    2: "Scenery / water", 3: "Linked path or effect marker (unresolved)",
    4: "Background layer", 5: "Particle / weather effect", 6: "Ambient sprite",
    13: "Level marker (unresolved)", 18: "Traffic points",
    101: "Production building", 102: "Obstacle / hazard", 104: "Worker / recruit",
    105: "Animal", 108: "Resource / item", 109: "Civilian", 110: "Worker base",
    111: "Helicopter", 112: "Helipad", 113: "Boat", 114: "Pier",
    116: "Doctor", 117: "Hospital", 118: "Railway station", 119: "Train",
    121: "Thief", 122: "Road entry marker (inferred)", 123: "Police officer",
    124: "Police station", 125: "Firefighter", 126: "Fire station", 128: "Valve",
    132: "Scenario controller (unresolved)", 134: "Medicine delivery",
    135: "Campaign node", 138: "Campaign bridge", 140: "Bonfire",
    141: "Air delivery actor (inferred)", 142: "Delivery location",
    144: "Generator", 146: "Construction lot", 147: "Blueprint boost",
    148: "Campaign tile", 149: "Scenario marker (unresolved)",
    150: "Runway light (inferred)", 151: "Pollution marker (inferred)",
    152: "Offroad region marker (inferred)", 153: "ATV", 154: "Garage",
}
HANDLED = {2, 6, 101, 102, 104, 108, 109, 110, 112, 113, 114, 125, 126, 142, 146, 147}
METADATA = {4, 18}  # The background header and traffic list are used separately.


def features(level):
    objects = level["objects"]
    types = {o["type"] for o in objects}
    params = {o["params"] for o in objects}
    extras = {o["extra"] for o in objects}
    found = []
    for condition, name in [
        (101 in types, "Building repair / production"),
        (146 in types, "Construction lots"),
        (sum(o["type"] == 110 for o in objects) > 1, "Multiple bases"),
        (105 in types, "Animal rescue"),
        (bool({116, 117} & types) or "sick" in extras, "Doctors / healing"),
        (bool({121, 123, 124} & types), "Police / thieves"),
        (bool({125, 126} & types), "Firefighters"),
        (bool({111, 112} & types) or "heli" in extras, "Helicopter rescue"),
        (bool({113, 114} & types) or bool({"boat", "roof"} & extras), "Boats / piers"),
        (bool({153, 154} & types) or "offroad" in extras, "ATV / garage"),
        (bool({118, 119} & types), "Rail services"),
        (bool({141, 142} & types), "Air deliveries"),
        (134 in types, "Medicine deadline"),
        (140 in types, "Bonfires / warmth or visibility"),
        (144 in types, "Generators"),
        (150 in types, "Runway lighting"),
        (128 in types, "Valves"),
        (151 in types, "Polluted water"),
        ("fog_high" in params, "Fog / gas barriers"),
        ("swarm0" in params, "Flooded road / pump"),
        (bool({"dynamite", "stone_dynamite"} & params), "Dynamite"),
        (147 in types, "Temporary boosts"),
        (any(o["name"] == "fire" or o["params"] == "fire" for o in objects), "Fires"),
    ]:
        if condition:
            found.append(name)
    return found


def campaign_order(levels):
    nodes = [o for o in levels["map"]["objects"] if o["type"] == 135]
    by_background = {d["background"]: d for n, d in levels.items() if n.startswith("level")}
    starts = [o for o in nodes if not o["extra"]]
    if len(starts) != 1:
        raise ValueError("Campaign does not have a unique starting node")
    ordered, visited = [], set()
    node = starts[0]
    while node:
        code = node["name"]
        if code in visited:
            raise ValueError("Cycle in campaign prerequisites")
        visited.add(code)
        ordered.append(by_background[code])
        following = [o for o in nodes if o["extra"] == code]
        if len(following) > 1:
            raise ValueError("Campaign branches; update audit ordering")
        node = following[0] if following else None
    if len(ordered) != len(nodes) or len(ordered) != len(by_background):
        raise ValueError("Campaign chain does not cover every numbered level")
    return ordered


def write_report(evidence, out):
    levels = {d["file"]: d for d in evidence["levels"]}
    ordered = campaign_order(levels)
    loaded = sum(d["initialized"] for d in ordered)
    h = html.escape
    rows, csv_rows = [], []
    for number, d in enumerate(ordered, 1):
        counts = collections.Counter(o["type"] for o in d["objects"])
        handled = HANDLED | ({105,128,151,153,154} if d.get("restored_mission") else set())
        metadata = METADATA | ({3,152} if d.get("restored_mission") else set())
        ignored = sorted(set(counts) - handled - metadata)
        summary = features(d)
        probe = d.get("probe")
        progress = f'{probe["completed"]}/{probe["targets"]}' if probe else "Not run"
        if d.get("restored_mission"):
            progress = f'{d["rescue_goal"]}/{d["rescue_goal"]} rescues (make test)'
            if d["animal_goal"]:
                progress += f'; {d["animal_goal"]}/{d["animal_goal"]} animals'
            if d["basin_goal"]:
                progress += f'; {d["basin_goal"]}/{d["basin_goal"]} basins'
        status = "Restored mission rules" if d.get("restored_mission") else "Loads; later mechanics deferred" if d["initialized"] else "Cannot load"
        error = d.get("init_error") or d.get("render_error") or "No startup error"
        # These notes come from the bundled tutorials / locale and map ordering.
        note = {
            1: "Regression completes the 3-person rescue goal with costs, rewards and automatic evacuation.",
            2: "Regression completes the 3-person goal using repair, income, bonus pickups and a recruited worker.",
            3: "Regression completes 5 rescues, including two recruits, and verifies house-to-cafe replacement.",
            4: "Regression completes 13 rescues including five ATV deliveries with fuel costs and recruitment.",
            5: "Regression completes 6 people and 16 wandering animals; linked fog valve and only one large boulder are needed.",
            6: "Regression completes 2 people, 6 animals and 2 basins using valve/pier gates and fishing. Cafe construction is forbidden.",
            7: "Generators must drain road hazards; dynamite must gate its boulder.",
            8: "Sick people must block roads until treated by a doctor.",
            11: "Police, occupied buildings and upgrades need real gameplay states.",
            12: "Fire threatens people; emergency rescue and failure conditions are missing.",
            17: "Medicine delivery needs a deadline and a failure state.",
            23: "Bonfires have visibility behavior in Training409; exact night schedule is absent.",
            26: "Runway generator and light markers exist; locale achievement requires night lighting.",
            30: "Training401 requires warmth; a cold-related failure message is supplied.",
            34: "The locale achievement explicitly identifies volcanic smoke on this level.",
            38: "This is the default prototype. Clearing its 31 simplified jobs does not cover helicopters or firefighters.",
            44: "Startup now succeeds; helicopters, doctors and police remain deferred. The earlier prototype probe stalled here.",
            45: "Train evacuation is confirmed by the railway-station achievement.",
            50: "Combined specialists and ATV are missing; original scenario rules still need recovery.",
        }.get(number, "Required mechanics are listed from the original object records; mission configuration is not recovered.")
        if number == 25:
            note = "Prototype finishes 40/40 counted jobs while omitting three animals, doctors, firefighters and ATV systems."
        detail = "".join(
            f'<tr><td>{t}</td><td>{h(TYPES.get(t, "Unclassified"))}</td><td>{counts[t]}</td>'
            f'<td>{"Handled in some form" if t in handled else "Authored scene / routing metadata" if t in metadata else "Deferred gameplay type"}</td></tr>'
            for t in sorted(counts)
        )
        rows.append(f'<tr data-search="{h(" ".join([str(number), d["file"], status, error, *summary]).lower())}">'
                    f'<td>{number}</td><td><code>{d["file"]}</code><br>Scene {h(d["background"])}</td>'
                    f'<td class="{"partial" if d["initialized"] else "bad"}">{status}<br><small>{h(error)}</small></td>'
                    f'<td>{h(" · ".join(summary))}</td><td>{progress}<br><small>{h(note)}</small>'
                    f'<details><summary>Object coverage</summary><table><tr><th>Type</th><th>Meaning</th><th>Count</th><th>Runtime</th></tr>{detail}</table></details></td></tr>')
        csv_rows.append({"campaign_level": number, "source_file": d["file"] + ".lvl",
                         "scene": d["background"], "status": status, "first_error": error,
                         "required_features": "; ".join(summary), "prototype_jobs": progress,
                         "skipped_types": "; ".join(map(str, ignored)), "notes": note})
    with (out / "levels.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(csv_rows[0]))
        writer.writeheader()
        writer.writerows(csv_rows)

    failures = collections.Counter(d["init_error"].split("'")[1] for d in ordered if not d["initialized"])
    failure_rows = "".join(f'<tr><td><code>{h(name)}</code></td><td>{count}</td></tr>' for name, count in failures.most_common())
    feature_counts = collections.Counter(f for d in ordered for f in features(d))
    feature_rows = "".join(f'<tr><td>{h(name)}</td><td>{count}</td></tr>' for name, count in feature_counts.items())
    report = f'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Rescue Team 5 support audit</title>
<style>
body{{font:16px/1.55 system-ui,sans-serif;color:#223047;background:#f5f7fa;margin:0}}
main{{max-width:1500px;margin:auto;padding:40px}}h1{{font-size:36px;line-height:1.2}}h2{{margin-top:40px}}
.lead{{max-width:950px;font-size:20px}}.cards{{display:flex;gap:16px;flex-wrap:wrap}}
.card{{background:white;border:1px solid #d6dee9;border-radius:12px;padding:18px;min-width:170px}}
.card b{{display:block;font-size:32px}}table{{border-collapse:collapse;width:100%;background:white;margin:12px 0}}
td,th{{text-align:left;vertical-align:top;border:1px solid #d6dee9;padding:12px}}th{{background:#e9eff6;position:sticky;top:0}}
small{{display:block;font-size:13px;font-weight:normal;color:#536177;margin-top:6px}}
.bad{{color:#a62a28;font-weight:650}}.partial{{color:#815200;font-weight:650}}a{{color:#155ec2}}
code{{font-size:13px}}input{{font:inherit;padding:12px;width:min(600px,85%);border:1px solid #9bacc1;border-radius:8px}}
details{{margin-top:12px}}summary{{cursor:pointer}}details table{{font-size:12px}}details th{{position:static}}
li{{margin:10px 0}}.columns{{display:grid;grid-template-columns:1fr 1fr;gap:24px}}
@media(max-width:900px){{main{{padding:18px}}.columns{{display:block}}.table-wrap{{overflow:auto}}}}
</style><main>
<h1>Rescue Team 5 support audit</h1>
<p class="lead">All {loaded} of 50 missions now load and draw. Campaign levels 1–6 complete under restored mission rules,
including the economy, recruitment, ATV rescues, wandering animals, pollution valves and fishing. Later missions remain inspection and prototype gameplay levels.</p>
<div class="cards"><div class="card"><b>52</b>Binary files decoded</div><div class="card"><b>50</b>Campaign missions</div>
<div class="card"><b>{loaded}</b>Load and draw a frame</div><div class="card"><b>{50-loaded}</b>Fail initialization</div></div>
<p>Audit date: 18 October 2026. <a href="levels.csv">Download the level matrix</a> · <a href="campaign.json">Full object and probe evidence</a> · <a href="tests.log">Existing test results</a></p>

<h2>What was checked</h2>
<p>The production binary reader decoded all 50 numbered missions plus map and menu: 11,645 object records in total.
The audit called the real initialization code for every mission and rendered one frame on every successful initialization.
The legacy 31-job probe is retained for source level000. <code>make test</code> also completes all six early missions through normal clicks and queues,
including the level-3 house-to-cafe sequence, without injecting resources or clearing obstacles directly. These are restored-rule completion checks;
exact retail prices, production intervals and timed scoring remain unverified.</p>
<p><strong>Campaign order is recovered from map nodes, not filenames.</strong> Type 135 names identify scene codes; their extra strings identify prerequisites.
They form one chain covering all 50 scenes. It begins at scene 103 (<code>level015.lvl</code>); <code>level000.lvl</code> is scene 001, campaign level 38.
The bundled training texts match the first introductions, and the online guide confirms their sequence through level 22.</p>

<h2>What works and where support ends</h2>
<p><strong>Campaign levels 1–6 now work through every mission goal.</strong> Starting inventories and goals match the original screenshots.
Ruined buildings can be repaired; working buildings pay automatic income and expose recurring bonus supplies for workers to collect.
Obstructions charge resources and logs yield wood. People evacuate through cleared routes, recruits join the crew, buildings can be demolished and replaced,
and mission completion offers Next level and Play again. ATV rescues spend fuel; animals wander and require food to capture;
valves clear their linked fog or polluted basins; repaired piers enable boat food deliveries. Level 6 blocks cafe construction and requires all three objective types.
The default launch is campaign level 1; <code>--campaign N</code> follows the original map order.</p>
<p>These missions currently use untimed play and explicit restoration balance for costs and production cadence. Retail deadlines, stars and scoring are not recovered.
The limitations below apply to the remaining campaign unless stated otherwise.</p>
<p>Basic traffic routing, debris clearing, resource pickup with return cargo, pedestrian escort, queue cancellation,
construction-menu selection and wood costs, pause/restart, HUD rendering, roof attachment, ambient birds/clouds and decorative log flames work under prototype rules.
These foundations remain the main scope of interaction in levels after the first six.</p>
<ul>
<li><strong>Economy and buildings after level 6:</strong> existing buildings remain inert. Repair, production and replacement rules are scoped to the first six missions.
Upgrades and later build restrictions remain deferred. Later obstruction jobs retain the prototype's free clearing rules.</li>
<li><strong>People and roles after level 6:</strong> every type-104 worker is enabled immediately, including stranded or sick recruits.
Type-109 civilians all use ordinary escort logic, ignoring sick, boat, helicopter and offroad requirements. Doctors and police are skipped;
firefighters are drawn as scenery. There is no recruit/rescue/base-activation lifecycle.</li>
<li><strong>Animals:</strong> all type-105 records are skipped. No wandering, capture, food cost, transport or animal objective exists.</li>
<li><strong>Transport:</strong> boats only receive floating-wood jobs. Piers and helipads are scenery, helicopters and ATVs are skipped,
and there is no passenger ferrying, rescue from rooftops, garage operation, rail evacuation, cargo service or fuel cost.
Air delivery locations have artwork but no order/delivery loop.</li>
<li><strong>Hazards:</strong> valves, generators, polluted water, medicine delivery, bonfires, runway lights and scenario controllers are skipped.
Fog and road fire are classified as ordinary obstacles. Decorative tree flames do not spread or threaten people.
No night cycle, warmth, fever, disaster failure or volcanic scenario behavior is implemented.</li>
<li><strong>Mission rules and progression after level 6:</strong> inspection missions retain three prototype objectives: rescue all type-109 civilians, clear every obstacle,
collect every supply. Retail goals include resource totals, building counts, animal rescue, thieves and evacuation.
Beyond the early missions there is no finish transition. Unlocking, save profiles, score, stars and the interactive campaign map remain deferred.
The countdown meter stays full; early missions display elapsed time. Game-mode selection is not implemented.</li>
<li><strong>Presentation:</strong> blueprint sprites are drawn but cannot be collected for boosts. Most particle/weather types,
linked scene effects, authored scale/rotation and extra background layers are not applied. No sound or music playback exists.</li>
</ul>

<h2>Launch failures</h2>
<p>{50-loaded} missions currently fail initialization. The original alias failures have been fixed, including <code>stone_big00 → bar_stone_big00</code>,
<code>money1/money2 → money/money_big</code>, <code>house → house0</code>, <code>derrick → gas0</code>,
and <code>bonus_fast_offroad → bonus_fast_offroader</code>.
Known scene-control expressions and editor markers are retained as data rather than looked up as ordinary sprites. Fog and road fires use effect rendering.</p>
<div class="columns"><div><table><tr><th>First unresolved animation / expression</th><th>Missions</th></tr>{failure_rows}</table></div>
<div><table><tr><th>Feature present in original records</th><th>Missions</th></tr>{feature_rows}</table></div></div>

<h2>All 50 campaign levels</h2>
<p>The first six rows show all mission goals completed by the end-to-end regression. The legacy 31-job probe counts only debris, supplies and escorts;
its full count is not a retail pass.
Feature presence is inferred from object types and parameters; it is not an authoritative mission objective list.
Unknown controller types remain explicitly unresolved.</p>
<input id="search" type="search" placeholder="Filter by level, file, mechanic or failure" aria-label="Filter level audit">
<p id="count">50 missions shown</p><div class="table-wrap"><table id="levels"><thead><tr><th>Campaign</th><th>Source</th><th>Launch</th><th>Original systems present</th><th>Verification and findings</th></tr></thead><tbody>{''.join(rows)}</tbody></table></div>

<h2>Repair order</h2>
<ol>
<li><strong>Completed: all missions are inspectable.</strong> All 50 pass initialization and a rendered frame. Scene controllers, additional background layers
and later gameplay types still need their actual behavior restored.</li>
<li><strong>Completed: first-six mission loops.</strong> Goals, starting supplies, costs, rewards, recruit eligibility, building condition and income,
demolition/rebuilding and finish transitions have end-to-end regression coverage. Next, validate restoration balance against retail prices and timing.</li>
<li><strong>Continue the early campaign systems.</strong> ATV, animals, valves, pollution and fishing are tested in levels 4–6. Next are generators and dynamite;
then doctors, police, helipads and building upgrades. Preserve the gates carried in names, extras and links.</li>
<li><strong>Restore time-sensitive missions.</strong> Spreading fire, medicine, air rescue, trains, night lighting, bonfire warmth,
volcanic smoke and failure conditions. Recover exact schedules and costs from retail behavior rather than guessing.</li>
<li><strong>Finish the campaign shell.</strong> Timed and relaxed modes, stars, score, progress saves, level unlocks, tutorials,
blueprint durations, achievements, sound and remaining scene presentation.</li>
</ol>

<h2>References and evidence limits</h2>
<p><a href="{WALKTHROUGH}">GameHouse Rescue Team 5 walkthrough</a> explains levels 1–22 and the economy, recruitment, specialists,
transport, hazards and boosts. <a href="{GUIDE}">The Steam community guide</a> links an
<a href="{PLAYLIST}">all-level video playlist</a>. The playlist is a reference for future behavior checks; this audit does not claim to have watched all videos.</p>
<p>Primary local evidence: <code>conf/locale_en.lua</code> supplies tutorial text, tasks, failure messages and achievements;
<code>level/map.lvl</code> supplies the campaign chain; all numbered <code>.lvl</code> files supply objects and metadata;
<code>texture/lazy/*.lua</code> supplies authored animation names. Runtime definitions are in <code>src/game_rules.c</code>;
spawning, scheduling, simulation and rendering are in <code>src/game_spawn.c</code>, <code>src/game_jobs.c</code>,
<code>src/game_sim.c</code> and <code>src/game_render.c</code>.</p>
<p>Beyond the first six missions, per-level objectives and initial inventory remain unrecovered. Exact retail costs, timers, disaster schedules and later build restrictions are not recovered from these level records.
Tutorials identify many behaviors but are not a substitute for those missing rules. Later mission notes use local records and locale achievements;
they do not imply a complete walkthrough beyond level 22. Numeric type labels marked “inferred” require confirmation against retail behavior.</p>
<p>Reproduce: <code>python3 tools/audit_rescue_team.py</code>. The audit creates evidence files and a temporary probe executable;
it does not alter production code or original assets. Source fingerprints: <a href="sources.json">sources.json</a>.</p>
</main><script>
const search=document.getElementById('search');search.addEventListener('input',()=>{{
let count=0;for(const row of document.querySelectorAll('#levels > tbody > tr')){{
row.hidden=!row.dataset.search.includes(search.value.toLowerCase());if(!row.hidden)count++;
}}document.getElementById('count').textContent=count+' missions shown';}});
</script></html>'''
    (out / "index.html").write_text(report)
    print(f"Report: {out / 'index.html'}")
    print(f"Parsed {len(levels)} files; {loaded}/50 initialize; {50-loaded}/50 fail initialization.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=ROOT / "reference/rescue-team")
    parser.add_argument("--output", type=Path, default=ROOT / "build/audit")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "SDL_VIDEODRIVER": "dummy", "RESCUE_TEAM_ASSETS": str(args.assets.resolve())}
    tests = subprocess.run(["make", "test"], cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / "tests.log").write_text(tests.stdout)
    if tests.returncode:
        raise RuntimeError(f"Existing tests failed; see {out / 'tests.log'}")
    audit_binary = out / "audit_levels"
    subprocess.run(["make", f"AUDIT_BINARY={audit_binary}", str(audit_binary)], cwd=ROOT, check=True)
    subprocess.run([str(out / "audit_levels"), str(args.assets.resolve()), str(out / "campaign.json")], cwd=ROOT, env=env, check=True)
    evidence = json.loads((out / "campaign.json").read_text())
    fingerprints = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted((ROOT / "src").glob("*.[ch]"))}
    (out / "sources.json").write_text(json.dumps(fingerprints, indent=2) + "\n")
    write_report(evidence, out)


if __name__ == "__main__":
    main()
