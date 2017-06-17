#ifndef GIMMMTEST_H
#define GIMMMTEST_H


#include <QObject>
#include <QtTest/QtTest>

class GimmmTest: public QObject
{
        Q_OBJECT
    private slots:
        void initTestCase();
};

#endif // GIMMMTEST_H
