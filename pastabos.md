# Svarbus netiesiskumo keitimo mechanizmas

base: 165202b70811a09142630a7d19d31077a39decad
head: 6febee342c3af73aa36987e03b7045c3a72fa9d4
status: ahead
ahead_by: 1
behind_by: 0
total_commits: 1


https://github.com/Karpavicius82/GNNv2 

Commit'inta tik 3 validuotas pure-physics probės (research/), 350 eilučių, 13/13:

┌────────────────────────────┬──────┬──────────────────────────────────────────────────────────────────────────────────────────┐
│           Failas           │ Rez. │                                           Esmė                                           │
├────────────────────────────┼──────┼──────────────────────────────────────────────────────────────────────────────────────────┤
│ probe_dynamic_flux.cpp     │ 4/4  │ DĖSNIS — gauge flux iš materijos srovės; Cayley flux + Cayley 2-vietės hop; norma ~1e-13 │
├────────────────────────────┼──────┼──────────────────────────────────────────────────────────────────────────────────────────┤
│ probe_spin_precession.cpp  │ 5/5  │ exp-be spin kick = Cayley-SO(3) precesija; ilgis tikslus be renorm                       │
├────────────────────────────┼──────┼──────────────────────────────────────────────────────────────────────────────────────────┤
│ probe_coriolis_observe.cpp │ 4/4  │ stebima (ne skaičiuojama) holonomija; Cayley-SO(3) rotoriai                              │
└────────────────────────────┴──────┴──────────────────────────────────────────────────────────────────────────────────────────┘

Nulis sqrt/cos/sin/exp — grep-patvirtinta.

Ką sąmoningai NEĮtraukiau (ir kodėl)

1. tools/graph_wave_nonlinear_engine.hpp (klone) — esminis tau/chi branduolio refaktoringas, ne mano ir mano nevaliduotas. Tai atskiras darbas (panašu Codex'o) → neturi keliauti su mano probėmis.
2. research/probe_spin_fabric.cpp — Codex'o spinor-fabric failas (tai „prieš" su exp). Neliečiau be jo žodžio.
3. tools/graph_wave_v3_feeling_gate_contract_test.cpp (main'e modifikuotas) — GNNv3 failas, atskira repozitorija → palikau ramybėje.

Jei nori, kad Codex'o spin_fabric (kaip „prieš"-etalonas) ar tas engine hpp taip pat būtų repozitorijoje — pasakyk, padarysiu atskirais, aiškiai atributuotais commit'ais (ne po mano autoryste). Kitaip — toliau perkeliam racionalų Cayley dėsnį ant viso streaming grafo?
