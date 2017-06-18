#include <QCoreApplication>

#include "application.h"
#include "unittests/gimmmtest.h"
#include <signal.h>

//#define UNIT_TEST_ON



#ifdef UNIT_TEST_ON
    QTEST_MAIN(GimmmTest)

#else
static int setup_posix_signal_handlers()
{
    struct sigaction hup, term;

    hup.sa_handler = Application::hupSignalHandler;
    sigemptyset(&hup.sa_mask);
    hup.sa_flags = 0;
    hup.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &hup, 0))
     return 1;

    term.sa_handler = Application::termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &term, 0))
     return 2;

    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Application app;

    setup_posix_signal_handlers();
    return a.exec();
}

#endif
