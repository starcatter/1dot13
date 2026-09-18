#ifndef POPUP_DEFINITION
	#define POPUP_DEFINITION

#include "LegacySGP.h"
#include "popup_class.h"
#include "popup_callback.h"

namespace popupGenerators{

	enum{
	dummy = 1,
	addArmor,
	addLBE,
	addWeapons,
	addWeaponGroups,
	addGrenades,
	addBombs,
	addFaceGear,
	addAmmo,
	addRifleGrenades,
	addRocketAmmo,
	addMisc,
	addKits
	};

};

class popupDef;
class popupDefContent;

class popupDefOption;
class popupDefSubPopupOption;
class popupDefContentGenerator;

class popupDef{
public:
	popupDef();
	~popupDef();

	BOOLEAN applyToBox(POPUP* popup);

	BOOLEAN addOption(ja2::text::Utf16String* name, UINT16 callbackId, UINT16 availId);

	popupDef * addSubPopup(ja2::text::Utf16String* name);
	BOOLEAN addSubPopup(popupDefSubPopupOption* sub);

	BOOLEAN addGenerator(UINT16 id);
protected:
	std::vector<popupDefContent*> content;
};

class popupDefContent{
public:
	popupDefContent();
	~popupDefContent();
	
	virtual BOOLEAN addToBox(POPUP * popup) = 0;

};

class popupDefOption : public popupDefContent{
public:
	popupDefOption() : name( new ja2::text::Utf16String(JA2_TEXT("Unnamed Option")) ), callbackId(0), availId(0){};
	popupDefOption( ja2::text::Utf16String* name, UINT16 callbackId, UINT16 availId ) : name( name ), callbackId(callbackId), availId(availId){};

	~popupDefOption(){ delete this->name; };

	BOOLEAN addToBox(POPUP * popup);

protected:
	ja2::text::Utf16String* name;
	UINT16 callbackId;
	UINT16 availId;

};

class popupDefSubPopupOption : public popupDefContent{
public:
	popupDefSubPopupOption() : name( new ja2::text::Utf16String(JA2_TEXT("Unnamed Submenu")) ){ this->content = new popupDef(); };
	popupDefSubPopupOption( ja2::text::Utf16String* name ) : name( name ){ this->content = new popupDef(); };

	~popupDefSubPopupOption(){ delete this->name; delete this->content; };

	BOOLEAN addToBox(POPUP * popup);
	void rename( ja2::text::Utf16String* name ){
		delete this->name;	// lets just hope nothing else was using this string. TODO: use smart pointer
		this->name = name;
	};
	popupDef * getSubDef(){ return this->content; };

protected:
	ja2::text::Utf16String* name;
	popupDef * content;
};

class popupDefContentGenerator: public popupDefContent{
public:
	popupDefContentGenerator() : generatorId(0){};
	popupDefContentGenerator( UINT16 generatorId ) : generatorId( generatorId ){};

	~popupDefContentGenerator();

	BOOLEAN addToBox(POPUP * popup);

protected:	
	UINT16 generatorId;
};

#endif
