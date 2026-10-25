#include "QuickOption.h"

class GameController;

class SandEffectOption: public QuickOption
{
public:
	SandEffectOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class DrawGravOption: public QuickOption
{
public:
	DrawGravOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class DecorationsOption: public QuickOption
{
public:
	DecorationsOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class NGravityOption: public QuickOption
{
public:
	NGravityOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class AHeatOption: public QuickOption
{
public:
	AHeatOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class ConsoleShowOption: public QuickOption
{
	GameController * c;
public:
	ConsoleShowOption(GameModel * m, GameController * c_);
	bool GetToggle() override;
	void perform() override;
};

// TPT Ultimata
class TimeDilationOption: public QuickOption {
public:
	TimeDilationOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class EMFieldOption: public QuickOption {
public:
	EMFieldOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class EMFieldElectricOption: public QuickOption {
public:
	EMFieldElectricOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class EMFieldMagneticOption: public QuickOption {
public:
	EMFieldMagneticOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class StressOption: public QuickOption {
public:
	StressOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

class StressViewOption: public QuickOption {
public:
	StressViewOption(GameModel * m);
	bool GetToggle() override;
	void perform() override;
};

