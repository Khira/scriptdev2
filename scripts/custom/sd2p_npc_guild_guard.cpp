/**
 *
 * @File : sd2p_npc_guild_guard.cpp
 *
 * @Authors : Wilibald09
 *
 * @Date : 13/06/2009
 *
 * @Version : 2.1
 *
 * @Synopsis : Gardiens des QG de guilde :
 *                 - Attaque les joueurs n'appartenant pas a la guilde correspondante.
 *                 - Teleporte les cadavres a des destinations random.
 *
 **/



#include "precompiled.h"
#include "sd2p_sc_npc_guild_guard.h"

using namespace SD2P_NAMESPACE;
using namespace SD2P_NAMESPACE::SD2P_GUILD_GUARD_NAMESPACE;


enum
{
    SPELL_DEATH             = 5,
    SPELL_DEATHALL          = 35354,

    MSG_AGGRO               = -2000000,
    MSG_DEATH               = -2000001,
    MSG_DEATHALL_PREVENT    = -2000002,
    MSG_DEATHALL            = -2000003,
    MSG_KILL_PLAYER         = -2000004,
};


struct MANGOS_DLL_DECL npc_guild_guard_base_ai : public ScriptedAI
{
    npc_guild_guard_base_ai(Creature * pCreature, Guard const * pGuard)
        : ScriptedAI(pCreature), m_pGuard(pGuard) {}

    Guard const * m_pGuard;


    void Reset(void)   {}
    void Aggro(Unit *) {}

    void MoveInLineOfSight(Unit * pWho)
    {
        if (!m_creature->IsHostileTo(pWho))
        {
            if (pWho->GetTypeId() != TYPEID_PLAYER)
                return;

            for (Guard::m_VGuilds_t::const_iterator It = m_pGuard->m_VGuilds.begin(); It != m_pGuard->m_VGuilds.end(); ++It)
                if (*It == ((Player*)pWho)->GetGuildId())
                    return;
        }

        if( !m_creature->getVictim() && pWho->isTargetableForAttack() && pWho->isInAccessablePlaceFor(m_creature))
        {
            if (m_creature->GetDistanceZ(pWho) > CREATURE_Z_ATTACK_RANGE)
                return;

            float attackRadius = m_creature->GetAttackDistance(pWho);
            if(m_creature->IsWithinDistInMap(pWho, attackRadius) && m_creature->IsWithinLOSInMap(pWho) )
            {
                pWho->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
                AttackStart(pWho);
            }
        }
    }

    void KilledUnit(Unit * pWho)
    {
        if (VGuardDest.empty() || pWho->GetTypeId() != TYPEID_PLAYER)
            return;

        if (Player * pPlayer = ((Player*)pWho)->GetSession()->GetPlayer())
        {
            uint32 DestId = VGuardDest[rand() % VGuardDest.size()];
            const Destination * pDest = GetDestination(DestId);
            if (pDest)
                pPlayer->TeleportTo(pDest->m_Map, pDest->m_X, pDest->m_Y, pDest->m_Z, pDest->m_Orient);
            else
                outstring_log("SD2P >> [Gardien Guilde] Destination introuvable (DestID: %u).", DestId);
        }
    }
};

struct MANGOS_DLL_DECL npc_guild_guard_ai : public npc_guild_guard_base_ai
{
    npc_guild_guard_ai(Creature * pCreature, Guard const * pGuard)
        : npc_guild_guard_base_ai(pCreature, pGuard)
    {
        Reset();
    }

    uint32 Death_Timer;
    uint32 DeathAll_Timer;
    bool   IsPhaseTwo;


    void Reset(void)
    {
        Death_Timer = 6000;
        DeathAll_Timer = 3000;
        IsPhaseTwo = false;
        m_creature->setFaction(35);
    }

    void Aggro(Unit *)
    {
        DoScriptText(MSG_AGGRO, m_creature);
        m_creature->setFaction(14);
    }

    void UpdateAI(const uint32 Diff)
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        if (!IsPhaseTwo)
        {
            if (Death_Timer < Diff)
            {
                DoCast(m_creature->getVictim(), SPELL_DEATH);
                Death_Timer = 5000;
            } else Death_Timer -= Diff;
        }
        else
        {
            if (DeathAll_Timer < Diff)
            {
                DoScriptText(MSG_DEATHALL, m_creature);
                DoCast(m_creature->getVictim(), SPELL_DEATHALL);
                DeathAll_Timer = 5000;
            } else DeathAll_Timer -= Diff;
        }

        if ((m_creature->GetHealth() * 100 / m_creature->GetMaxHealth()) < 30 && !IsPhaseTwo)
        {
            DoScriptText(MSG_DEATHALL_PREVENT, m_creature);
            IsPhaseTwo = true;
        }

        DoMeleeAttackIfReady();
    }

    void KilledUnit(Unit * pWho)
    {
        npc_guild_guard_base_ai::KilledUnit(pWho);

        if (IsPhaseTwo) return;
        DoScriptText(MSG_KILL_PLAYER, m_creature, pWho);
    }

    void JustDied(Unit*)
    {
        DoScriptText(MSG_DEATH, m_creature);
    }
};

CreatureAI* GetAI_npc_guild_guard(Creature * pCreature)
{
    const Guard * pGarde = GetGuard(pCreature->GetEntry());
    if (!pGarde)
    {
        outstring_log("SD2P >> [Gardien Guilde] Aucune Guilde associee (entry: %u).", pCreature->GetEntry());
        return NULL;
    }
    return new npc_guild_guard_ai(pCreature, pGarde);
}


void AddSC_npc_guild_guard(void)
{
    Script * newscript;

    newscript = new Script;
    newscript->Name="sd2p_npc_guild_guard";
    newscript->GetAI = &GetAI_npc_guild_guard;
    newscript->RegisterSelf();
}
