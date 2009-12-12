/**
 *
 * @File : sd2p_npc_teleport.cpp
 *
 * @Authors : Wilibald09
 *
 * @Date : 14/06/2009
 *
 * @Version : 2.0
 *
 **/


#include "precompiled.h"
#include "sd2p_sc_npc_teleport.h"

#define NEXT_PAGE  "-> Page suivante"
#define PREV_PAGE  "<- Page precedente"
#define MAIN_MENU  "<= [Menu Principal]"

using namespace std;
using namespace SD2P_NAMESPACE;
using namespace SD2P_NAMESPACE::SD2P_NPC_TELE_NAMESPACE;


namespace
{
    enum
    {
        // npc_text
        NPC_TEXT_NOTHING        = 100000,
        NPC_TEXT_CAT            = 100001,
        NPC_TEXT_DEST           = 100002,

        // Divers
        GOSSIP_SHOW_DEST        = GOSSIP_ACTION_INFO_DEF + 1,
        GOSSIP_TELEPORT         = GOSSIP_ACTION_INFO_DEF + 2,
        GOSSIP_NEXT_PAGEC       = GOSSIP_ACTION_INFO_DEF + 3,
        GOSSIP_PREV_PAGEC       = GOSSIP_ACTION_INFO_DEF + 4,
        GOSSIP_NEXT_PAGED       = GOSSIP_ACTION_INFO_DEF + 5,
        GOSSIP_PREV_PAGED       = GOSSIP_ACTION_INFO_DEF + 6,
        GOSSIP_MAIN_MENU        = GOSSIP_ACTION_INFO_DEF + 7,
    };

    ListPage PageC, PageD, CatSelect;


    // Fonction d'affichage des categories
    void AffichCat(Player * const pPlayer, Creature * const pCreature)
    {
        // Affichage Page Precedente.
        if (PageC[pPlayer] > 0)
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, PREV_PAGE, GOSSIP_PREV_PAGEC, 0);

        uint32 NbDest = 0;
        VCategorie_t::size_type i = PageC[pPlayer] * NB_ITEM_PAGE;
        for ( ; NbDest < NB_ITEM_PAGE && i < VCategorie.size(); ++i)
        {
            const Categorie * pCat = &VCategorie[i];
            if (!pCat->IsAllowedToTeleport(pPlayer)) continue;
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, pCat->GetName(pPlayer->isGameMaster()).c_str(), GOSSIP_SHOW_DEST, i);
            ++NbDest;
        }

        // Affichage Page Suivante.
        if (i < VCategorie.size() && NbDest == NB_ITEM_PAGE)
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, NEXT_PAGE, GOSSIP_NEXT_PAGEC, 0);

        pPlayer->SEND_GOSSIP_MENU((NbDest ? NPC_TEXT_CAT : NPC_TEXT_NOTHING), pCreature->GetGUID());
    }

    // Fonction d'affichage des destinations d'une categorie
    void AffichDest(Player * const pPlayer, Creature * const pCreature)
    {
        // Affichage Page Precedente.
        if (PageD[pPlayer] > 0)
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, PREV_PAGE, GOSSIP_PREV_PAGED, 0);

        const Categorie * pCat = &VCategorie[CatSelect[pPlayer]];
        MDestination_t::key_type i = PageD[pPlayer] * NB_ITEM_PAGE;
        for ( ; i < pCat->size() && i < NB_ITEM_PAGE * (PageD[pPlayer] + 1); ++i)
        {
            const Destination * pDest = pCat->GetDest(i);
            if (pDest)
                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_INTERACT_2, pDest->m_Name.c_str(), GOSSIP_TELEPORT, pDest->m_Id);
            else
                outstring_log("SD2P >> [NPC Tele] Destination introuvable (DestID: %u).", i);
        }

        // Affichage Page Suivante.
        if (i < pCat->size())
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, NEXT_PAGE, GOSSIP_NEXT_PAGED, 0);

        // Affichage lien vers menu Principal.
        if (CountOfCategoryAllowedBy(pPlayer) > 1)
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, MAIN_MENU, GOSSIP_MAIN_MENU, 0);

        pPlayer->SEND_GOSSIP_MENU(NPC_TEXT_DEST, pCreature->GetGUID());
    }

    // Fonction de verification avant teleportation
    void ActionTeleport(Player * const pPlayer, Creature * const pCreature, const uint32 &DestId)
    {
        const Destination * pDest = GetDestination(DestId);
        ostringstream oss;

        // Si le joueur est un Maitre du Jeu, teleportation immediate.
        if (pPlayer->isGameMaster())
        {
            Teleport(pPlayer, pDest->m_Map, pDest->m_X, pDest->m_Y, pDest->m_Z, pDest->m_Orient);
            return;
        }

        // Verification niveau joueur.
        if (pPlayer->getLevel() < pDest->m_Level) 
        {
            oss << "Vous n'avez pas le niveau requis. Cette destination requiert le niveau "
                << static_cast<uint32>(pDest->m_Level) << ".";
            pCreature->MonsterWhisper(oss.str().c_str(), pPlayer->GetGUID());
            return;
        }

        // Verification argent requis.
        if (pPlayer->GetMoney() < pDest->m_Cost)
        {
            oss << "Vous n'avez pas assez d'argent. Le prix de la teleportation est de "
                << pDest->m_Cost << ".";
            pCreature->MonsterWhisper(oss.str().c_str(), pPlayer->GetGUID());
            return;
        }

        // Retrait de l'argent si necessaire.
        if (pDest->m_Cost)
            pPlayer->ModifyMoney(-1 * pDest->m_Cost);

        // Teleportation du joueur.
        Teleport(pPlayer, pDest->m_Map, pDest->m_X, pDest->m_Y, pDest->m_Z, pDest->m_Orient);
    }
}

bool GossipHello_npc_teleport(Player * pPlayer, Creature * pCreature)
{
    // Re-initialisation des pages pour le joueur.
    PageC[pPlayer] = PageD[pPlayer] = CatSelect[pPlayer] = 0;

    // Verification si joueur en combat.
    if(pPlayer->isInCombat())
    {
        pPlayer->CLOSE_GOSSIP_MENU();
        pCreature->MonsterWhisper("Vous etes en combat. Revenez plus tard", pPlayer->GetGUID());
        return true;
    }

    // Affichage des categories.
    AffichCat(pPlayer, pCreature);
    return true;
}

bool GossipSelect_npc_teleport(Player * pPlayer, Creature * pCreature, uint32 Sender, uint32 Param)
{
    switch(Sender) 
    {
      // Affichage des destinations.
      case GOSSIP_SHOW_DEST:
        CatSelect[pPlayer] = Param;
        AffichDest(pPlayer, pCreature);
        break;

      // Page de categories precedente.
      case GOSSIP_PREV_PAGEC:
        --PageC[pPlayer];
        AffichCat(pPlayer, pCreature);
        break;

      // Page de categories suivante.
      case GOSSIP_NEXT_PAGEC:
        ++PageC[pPlayer];
        AffichCat(pPlayer, pCreature);
        break;

      // Page de destinations precedente.
      case GOSSIP_PREV_PAGED:
        --PageD[pPlayer];
        AffichDest(pPlayer, pCreature);
        break;

      // Page de destinations suivante.
      case GOSSIP_NEXT_PAGED:
        ++PageD[pPlayer];
        AffichDest(pPlayer, pCreature);
        break;

      // Affichage Menu principal.
      case GOSSIP_MAIN_MENU:
        GossipHello_npc_teleport(pPlayer, pCreature);
        break;

      // Teleportation.
      case GOSSIP_TELEPORT:
        pPlayer->CLOSE_GOSSIP_MENU();
        ActionTeleport(pPlayer, pCreature, Param);
        break;
    }
    return true;
}


void AddSC_npc_teleport(void)
{
    Script * newscript;

    newscript = new Script;
    newscript->Name="sd2p_npc_teleport";
    newscript->pGossipHello =  &GossipHello_npc_teleport;
    newscript->pGossipSelect = &GossipSelect_npc_teleport;
    newscript->RegisterSelf();
}
