//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "monsters.h"
#include "monster.h"
#include "container.h"
#include "tools.h"
#include "spells.h"
#include "luascript.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

extern Spells* g_spells;

MonsterType::MonsterType()
{
	reset();
}

void MonsterType::reset()
{
	race = RACE_BLOOD;
	armor = 0;
	experience = 0;
	defense = 0;
	hasDistanceAttack = false;
	canPushItems = false;
	staticLook = 1;
	staticAttack = 1;
	changeTargetChance = 1;
	maxSummons = 0;
	targetDistance = 1;
	runAwayHealth = 0;
	pushable = true;
	base_speed = 200;
	
	health = 100;
	health_max = 100;
	lookhead = 10;
	lookbody = 10;
	looklegs = 10;
	lookfeet = 10;
	looktype = 10;
	lookcorpse = 0;
	lookmaster = 10;
	damageImmunities = 0;
	conditionImmunities = 0;
	lightLevel = 0;
	lightColor = 0;

	combatMeleeMin = 0;
	combatMeleeMax = 0;

	summonList.clear();
	lootItems.clear();
	spellAttackList.clear();
	spellDefenseList.clear();

	yellChance = 0;
	voiceVector.clear();
}

MonsterType::~MonsterType()
{
	//
}

unsigned long Monsters::getRandom()
{
	return (unsigned long)((rand()<< 16 | rand()) % CHANCE_MAX);
}

void MonsterType::createLoot(Container* corpse)
{
	LootItems::const_iterator it;
	for(it = lootItems.begin(); it != lootItems.end(); it++){
		Item* tmpItem = createLootItem(*it);
		if(tmpItem){
			//check containers
			if(Container* container = dynamic_cast<Container*>(tmpItem)){
				createLootContainer(container, *it);
				if(container->size() == 0){
					delete container;
				}
				else{
					corpse->__internalAddThing(tmpItem);
				}
			}
			else{
				corpse->__internalAddThing(tmpItem);
			}
		}
	}
}

Item* MonsterType::createLootItem(const LootBlock& lootBlock)
{
	Item* tmpItem = NULL;
	if(Item::items[lootBlock.id].stackable == true){
		unsigned long randvalue = Monsters::getRandom();
		unsigned long n = 1;
		if(randvalue < lootBlock.chance1){
			if(randvalue < lootBlock.chancemax){
				n = lootBlock.countmax;
			}
			else{
				//if chancemax < randvalue < chance1
				n = (unsigned char)(randvalue % lootBlock.countmax + 1);
			}		
			tmpItem = Item::CreateItem(lootBlock.id,n);
		}
	}
	else{
		if(Monsters::getRandom() < lootBlock.chance1){
			tmpItem = Item::CreateItem(lootBlock.id);
		}
	}
	return tmpItem;
}

void MonsterType::createLootContainer(Container* parent, const LootBlock& lootblock)
{
	LootItems::const_iterator it;
	for(it = lootblock.childLoot.begin(); it != lootblock.childLoot.end(); it++){
		Item* tmpItem = createLootItem(*it);
		if(tmpItem){
			if(Container* container = dynamic_cast<Container*>(tmpItem)){
				createLootContainer(container, *it);
				if(container->size() == 0){
					delete container;
				}
				else{
					parent->__internalAddThing(dynamic_cast<Thing*>(container));
				}
			}
			else{
				parent->__internalAddThing(tmpItem);
			}
		}
	}
}

Monsters::Monsters()
{
	loaded = false;
}

bool Monsters::loadFromXml(const std::string& _datadir,bool reloading /*= false*/)
{	
	loaded = false;	
	datadir = _datadir;
	
	std::string filename = datadir + "monster/monsters.xml";

	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if(doc){
		loaded = true;
		xmlNodePtr root, p;
		unsigned long id = 0;
		root = xmlDocGetRootElement(doc);

		if(xmlStrcmp(root->name,(const xmlChar*)"monsters") != 0){
			xmlFreeDoc(doc);
			loaded = false;
			return false;
		}

		p = root->children;
		while(p){
			if(xmlStrcmp(p->name, (const xmlChar*)"monster") == 0){
				std::string file;
				std::string name;

				if(readXMLString(p, "file", file) && readXMLString(p, "name", name)){
					file = datadir + "monster/" + file;
					
					std::string lowername = name;
					toLowerCaseString(lowername);
						
					MonsterType* mType = loadMonster(file, name, reloading);
					if(mType){
						id++;
						monsterNames[lowername] = id;
						monsters[id] = mType;
					}
				}
			}
			p = p->next;
		}
		
		xmlFreeDoc(doc);
	}

	return loaded;
}

bool Monsters::reload()
{
	return loadFromXml(datadir, true);
}

MonsterType* Monsters::loadMonster(const std::string& file,const std::string& monster_name, bool reloading /*= false*/)
{
	bool monsterLoad;
	MonsterType* mType = NULL;
	bool new_mType = true;
	
	if(reloading){
		unsigned long id = getIdByName(monster_name);
		if(id != 0){
			mType = getMonsterType(id);
			if(mType != NULL){
				new_mType = false;
				mType->reset();
			}
		}
	}
	if(new_mType){
		mType = new MonsterType;
	}
	
	monsterLoad = true;
	xmlDocPtr doc = xmlParseFile(file.c_str());

	if(doc){
		xmlNodePtr root, p;
		root = xmlDocGetRootElement(doc);

		if(xmlStrcmp(root->name,(const xmlChar*)"monster") != 0){
			std::cerr << "Malformed XML: " << file << std::endl;
		}

		int intValue;
		std::string strValue;

		p = root->children;

		if(readXMLString(root, "name", strValue)){
			mType->name = strValue;
		}
		else
			monsterLoad = false;

		if(readXMLString(root, "nameDescription", strValue)){
			mType->nameDescription = strValue;
		}
		else{
			mType->nameDescription = "a " + mType->name;
			toLowerCaseString(mType->nameDescription);
		}

		if(readXMLInteger(root, "experience", intValue)){
			mType->experience = intValue;
		}

		if(readXMLInteger(root, "pushable", intValue)){
			mType->pushable = (intValue != 0);
		}

		if(readXMLInteger(root, "speed", intValue)){
			mType->base_speed = intValue;
		}

		if(readXMLInteger(root, "defense", intValue)){
			mType->defense = intValue;
		}

		if(readXMLInteger(root, "armor", intValue)){
			mType->armor = intValue;
		}

		if(readXMLInteger(root, "canpushitems", intValue)){
			mType->canPushItems = (intValue != 0);
		}

		if(readXMLInteger(root, "staticattack", intValue)){
			if(intValue == 0)
				mType->staticAttack = 1;
			else if(intValue >= RAND_MAX)
				mType->staticAttack = RAND_MAX;
			else
				mType->staticAttack = intValue;
		}

		if(readXMLInteger(root, "changetarget", intValue)){
			//0	never, 10000 always
			mType->changeTargetChance = intValue;
		}

		if(readXMLInteger(root, "lightlevel", intValue)){
			mType->lightLevel = intValue;
		}

		if(readXMLInteger(root, "lightcolor", intValue)){
			mType->lightColor = intValue;
		}

		while(p){
			if(xmlStrcmp(p->name, (const xmlChar*)"health") == 0){

				if(readXMLInteger(p, "now", intValue)){
					mType->health = intValue;
				}
				else
					monsterLoad = false;

				if(readXMLInteger(p, "max", intValue)){
					mType->health_max = intValue;
				}
				else
					monsterLoad = false;
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"combat") == 0){

				if(readXMLInteger(p, "targetdistance", intValue)){
					mType->targetDistance = std::max(1, intValue);
				}

				if(readXMLInteger(p, "runonhealth", intValue)){
					mType->runAwayHealth = intValue;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"look") == 0){

				if(readXMLInteger(p, "type", intValue)){
					mType->looktype = intValue;
					mType->lookmaster = mType->looktype;
				}

				if(readXMLInteger(p, "head", intValue)){
					mType->lookhead = intValue;
				}

				if(readXMLInteger(p, "body", intValue)){
					mType->lookbody = intValue;
				}

				if(readXMLInteger(p, "legs", intValue)){
					mType->looklegs = intValue;
				}

				if(readXMLInteger(p, "feet", intValue)){
					mType->lookfeet = intValue;
				}

				if(readXMLInteger(p, "corpse", intValue)){
					mType->lookcorpse = intValue;
				}

				if(readXMLInteger(p, "race", intValue)){
					switch(intValue){
						case 1:
							mType->race = RACE_VENOM;
							break;
						case 2:
							mType->race = RACE_BLOOD;
							break;

						case 3:
							mType->race = RACE_UNDEAD;
							break;
					}
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"attacks") == 0){
				xmlNodePtr tmpNode = p->children;
				while(tmpNode){
					if(xmlStrcmp(tmpNode->name, (const xmlChar*)"attack") == 0){
						
						int32_t min = 0;
						int32_t max = 0;
						int32_t chance = 100;
						int32_t speed = 1000;

						if(readXMLInteger(tmpNode, "speed", intValue)){
							speed = intValue;
						}

						if(readXMLInteger(tmpNode, "chance", intValue)){
							chance = intValue;
						}

						if(readXMLInteger(tmpNode, "min", intValue)){
							min = intValue;
						}

						if(readXMLInteger(tmpNode, "max", intValue)){
							max = intValue;
						}

						if(readXMLString(tmpNode, "name", strValue)){
							if(strcasecmp(strValue.c_str(), "melee") == 0){
								mType->combatMeleeMin = min;
								mType->combatMeleeMax = min;
								mType->combatMeleeSpeed = speed;
							}
							else{
								spellBlock_t sb;
								sb.chance = chance;
								sb.minCombatValue = min;
								sb.maxCombatValue = max;
								sb.speed = speed;

								Spell* spell;
								if(spell = g_spells->getSpellByName(strValue)){
									sb.spell = spell;
									mType->spellAttackList.push_back(sb);
								}
								else{
									std::cout << "Warning: [Monsters::loadMonster] Spell not found - " << file << std::endl;
								}
							}
						}
						else{
							std::cout << "Warning: [Monsters::loadMonster] name attribute is missing - " << file << std::endl;
						}
					}

					tmpNode = tmpNode->next;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"defenses") == 0){
				xmlNodePtr tmpNode = p->children;
				while(tmpNode){
					if(xmlStrcmp(tmpNode->name, (const xmlChar*)"defense") == 0){
						
						int32_t min = 0;
						int32_t max = 0;
						int32_t chance = 100;
						int32_t speed = 1000;

						if(readXMLInteger(tmpNode, "speed", intValue)){
							speed = intValue;
						}

						if(readXMLInteger(tmpNode, "chance", intValue)){
							chance = intValue;
						}

						if(readXMLInteger(tmpNode, "min", intValue)){
							min = intValue;
						}

						if(readXMLInteger(tmpNode, "max", intValue)){
							max = intValue;
						}

						if(readXMLString(tmpNode, "name", strValue)){
							spellBlock_t sb;
							sb.chance = chance;
							sb.minCombatValue = min;
							sb.maxCombatValue = max;
							sb.speed = speed;

							Spell* spell;
							if(spell = g_spells->getSpellByName(strValue)){
								sb.spell = spell;
								mType->spellDefenseList.push_back(sb);
							}
							else{
								std::cout << "Warning: [Monsters::loadMonster] Spell not found - " << file << std::endl;
							}
						}
						else{
							std::cout << "Warning: [Monsters::loadMonster] name attribute is missing - " << file << std::endl;
						}
					}

					tmpNode = tmpNode->next;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"immunities") == 0){
				xmlNodePtr tmpNode = p->children;
				while(tmpNode){
					if(xmlStrcmp(tmpNode->name, (const xmlChar*)"immunity") == 0){

						if(readXMLString(tmpNode, "name", strValue)){

							if(strcasecmp(strValue.c_str(), "energy") == 0){
								mType->damageImmunities |= DAMAGE_ENERGY;
								mType->conditionImmunities |= CONDITION_ENERGY;
							}
							else if(strcasecmp(strValue.c_str(), "fire") == 0){
								mType->damageImmunities |= DAMAGE_FIRE;
								mType->conditionImmunities |= CONDITION_FIRE;
							}
							else if(strcasecmp(strValue.c_str(), "poison") == 0){
								mType->damageImmunities |= DAMAGE_POISON;
								mType->conditionImmunities |= CONDITION_POISON;
							}
							else if(strcasecmp(strValue.c_str(), "lifedrain") == 0){
								mType->damageImmunities |= DAMAGE_LIFEDRAIN;
								mType->conditionImmunities |= CONDITION_LIFEDRAIN;
							}
							else if(strcasecmp(strValue.c_str(), "paralyze") == 0){
								mType->conditionImmunities |= CONDITION_PARALYZE;
							}
							else if(strcasecmp(strValue.c_str(), "outfit") == 0){
								mType->conditionImmunities |= CONDITION_OUTFIT;
							}
							else if(strcasecmp(strValue.c_str(), "drunk") == 0){
								mType->conditionImmunities |= CONDITION_DRUNK;
							}
						}
					}

					tmpNode = tmpNode->next;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"voices") == 0){
				xmlNodePtr tmpNode = p->children;
				
				if(readXMLInteger(p, "speed", intValue)){
					mType->yellSpeedTicks = intValue;
				}

				if(readXMLInteger(p, "chance", intValue)){
					mType->yellChance = intValue;
				}

				while(tmpNode){
					if(xmlStrcmp(tmpNode->name, (const xmlChar*)"voice") == 0) {
						if(readXMLString(tmpNode, "sentence", strValue)){
							//vb.text = strValue;
							mType->voiceVector.push_back(strValue);
						}
					}

					tmpNode = tmpNode->next;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"loot") == 0){
				xmlNodePtr tmpNode = p->children;
				while(tmpNode){
					LootBlock lootBlock;
					if(loadLootItem(tmpNode, lootBlock)){
						mType->lootItems.push_back(lootBlock);
					}

					tmpNode = tmpNode->next;
				}
			}
			else if(xmlStrcmp(p->name, (const xmlChar*)"summons") == 0){

				if(readXMLInteger(p, "maxSummons", intValue)){
					mType->maxSummons = std::min(intValue, 100);
				}

				xmlNodePtr tmpNode = p->children;
				while(tmpNode){

					if(xmlStrcmp(tmpNode->name, (const xmlChar*)"summon") == 0){
						summonBlock_t sb;
						sb.name = "";
						sb.chance = CHANCE_MAX;

						if(readXMLString(tmpNode, "name", strValue)){
							sb.name = strValue;
						}
						else
							continue;

						if(readXMLInteger(tmpNode, "chance", intValue)){
							sb.chance = std::max(intValue, 100);
							if(sb.chance > CHANCE_MAX)
								sb.chance = CHANCE_MAX;
						}

						mType->summonList.push_back(sb);
					}

					tmpNode = tmpNode->next;
				}
			}
			p = p->next;
		}

		xmlFreeDoc(doc);
	}
	else{
		monsterLoad = false;
	}

	if(monsterLoad){
		return mType;
	}
	else{
		delete mType;
		return NULL;
	}
}

bool Monsters::loadLootItem(xmlNodePtr node, LootBlock& lootBlock)
{
	int intValue;
	if(readXMLInteger(node, "id", intValue)){
		lootBlock.id = intValue;
	}
	
	if(lootBlock.id == 0){
		return false;
	}
	
	if(Item::items[lootBlock.id].stackable == true){
		if(readXMLInteger(node, "countmax", intValue)){
			lootBlock.countmax = intValue;

			if(lootBlock.countmax > 100){
				lootBlock.countmax = 100;
			}
		}
		else{
			std::cout << "missing countmax for loot id = "<< lootBlock.id << std::endl;
			lootBlock.countmax = 1;
		}
			
		if(readXMLInteger(node, "chancemax", intValue)){
			lootBlock.chancemax = intValue;

			if(lootBlock.chancemax > CHANCE_MAX){
				lootBlock.chancemax = 0;
			}
		}
		else{
			std::cout << "missing chancemax for loot id = "<< lootBlock.id << std::endl;
			lootBlock.chancemax = 0;
		}

		if(readXMLInteger(node, "chance1", intValue)){
			lootBlock.chance1 = intValue;

			if(lootBlock.chance1 > CHANCE_MAX){
				lootBlock.chance1 = CHANCE_MAX;
			}
			
			if(lootBlock.chance1 <= lootBlock.chancemax){
				std::cout << "Wrong chance for loot id = "<< lootBlock.id << std::endl;
				return false;
			}
		}
		else{
			std::cout << "missing chance1 for loot id = "<< lootBlock.id << std::endl;
			lootBlock.chance1 = CHANCE_MAX;
		}
	}
	else{
		if(readXMLInteger(node, "chance", intValue)){
			lootBlock.chance1 = intValue;

			if(lootBlock.chance1 > CHANCE_MAX){
				lootBlock.chance1 = CHANCE_MAX;
			}
		}
		else{
			std::cout << "missing chance for loot id = "<< lootBlock.id << std::endl;
			lootBlock.chance1 = CHANCE_MAX;
		}
	}

	if(Item::items[lootBlock.id].isContainer()){
		loadLootContainer(node, lootBlock);
	}
	
	return true;
}

bool Monsters::loadLootContainer(xmlNodePtr node, LootBlock& lBlock)
{
	if(node == NULL){
		return false;
	}
	
	xmlNodePtr tmpNode = node->children;
	xmlNodePtr p;

	if(tmpNode == NULL){
		return false;
	}

	while(tmpNode){
		if(xmlStrcmp(tmpNode->name, (const xmlChar*)"inside") == 0){
			p = tmpNode->children;
			while(p){
				LootBlock lootBlock;
				if(loadLootItem(p, lootBlock)){
					lBlock.childLoot.push_back(lootBlock);
				}
				p = p->next;
			}
			return true;
		}//inside

		tmpNode = tmpNode->next;
	}

	return false;	
}

MonsterType* Monsters::getMonsterType(unsigned long mid)
{
	MonsterMap::iterator it = monsters.find(mid);
	if(it != monsters.end()){
		return it->second;
	}
	else{
		return NULL;
	}
}

unsigned long Monsters::getIdByName(const std::string& name)
{
	std::string lower_name = name;
	std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), tolower);
	MonsterNameMap::iterator it = monsterNames.find(lower_name);
	if(it != monsterNames.end()){
		return it->second;
	}
	else{
		return 0;
	}
}

Monsters::~Monsters()
{
	for(MonsterMap::iterator it = monsters.begin(); it != monsters.end(); it++)
		delete it->second;
}
