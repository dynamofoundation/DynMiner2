#include "cBlockMonitor.h"


void cBlockMonitor::runMonitor() {

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

}