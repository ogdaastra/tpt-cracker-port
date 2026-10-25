#pragma once
#include "Config.h"

#include "common/String.h"

#include <vector>

class GameModel;
class QuickOption;
class QuickOptionListener
{
protected:
	QuickOptionListener() {}
public:
	virtual ~QuickOptionListener() {}
	virtual void OnValueChanged(QuickOption * sender) {}
};
class QuickOption
{
public:
	enum Type {
		Toggle, Multi
	};
protected:
	std::vector<QuickOptionListener*> listeners;
	GameModel * m;
	Type type;
	String icon;
	String description;
	bool left_bottom; // TPT Ultimata: shown in the second side-bar column
	QuickOption(String icon, String description, GameModel * m, Type type, bool left_bottom=false) :
		m(m),
		type(type),
		icon(icon),
		description(description),
		left_bottom(left_bottom)
	{

	}
	virtual void perform() {}
public:
	virtual ~QuickOption()
	{
		for(std::vector<QuickOptionListener*>::iterator iter = listeners.begin(), end = listeners.end(); iter != end; ++iter)
			delete *iter;
	}

	std::vector<QuickOptionListener*> GetListeners()
	{
		return listeners;
	}

	void AddListener(QuickOptionListener * listener)
	{
		listeners.push_back(listener);
	}

	Type GetType() { return type; }

	virtual bool GetToggle() { return true;}
	virtual int GetMutli() { return 0;}
	virtual int GetMultiCount() { return 0;}

	String GetIcon() { return icon; }
	void SetIcon(String icon) { this->icon = icon; }
	bool GetBottomLeft() { return left_bottom; }
	String GetDescription() { return description; }
	void SetDescription(String description) { this->description = description; }
	void Perform()
	{
		perform();
		for(std::vector<QuickOptionListener*>::iterator iter = listeners.begin(), end = listeners.end(); iter != end; ++iter)
			(*iter)->OnValueChanged(this);
	}
	void Update()
	{
		for(std::vector<QuickOptionListener*>::iterator iter = listeners.begin(), end = listeners.end(); iter != end; ++iter)
			(*iter)->OnValueChanged(this);
	}
};

