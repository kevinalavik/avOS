#include <Adk/AdkMain.h>
#include <Lib/String.h>

static Bool TestInitialize(AdkAppContext *Context)
{
	AdkAppPostLine(Context, "Hello from avOS! anton is a stinky jew");
	return True;
}

int main(void)
{
	AdkApplication Application;

	Application.Title = "avOS Text Test";
	Application.WindowWidth = 240;
	Application.WindowHeight = 120;
	Application.WindowKind = AdkAetherWindowKindText;
	Application.Initialize = TestInitialize;
	Application.Update = 0;
	Application.CharInput = 0;
	Application.Pulse = 0;
	Application.Shutdown = 0;

	return AdkRunApplication(&Application);
}
