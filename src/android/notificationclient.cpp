#include "notificationclient.h"


#if defined(Q_OS_ANDROID)
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QtAndroid>
#include <QException>

class InterfaceConnFailedException : public QException
{
public:
    void raise() const { throw *this; }
    InterfaceConnFailedException *clone() const { return new InterfaceConnFailedException(*this); }
};
#elif defined(Q_OS_WINDOWS)
#elif defined(Q_OS_DARWIN)
#else
#endif


NotificationClient::NotificationClient(QObject *parent)
    : QObject(parent)
{
    connect(this, SIGNAL(notificationChanged()), this, SLOT(updateAndroidNotification()));
}

void NotificationClient::notify(const QString &notification)
{
    if (m_notification == notification)
        return;

    m_notification = notification;
    emit notificationChanged();
}

QString NotificationClient::notification() const
{
    return m_notification;
}

void NotificationClient::updateAndroidNotification()
{

#if defined(Q_OS_ANDROID)
    QAndroidJniEnvironment _env;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");   //activity is valid
    if (_env->ExceptionCheck()) {
        _env->ExceptionClear();
        throw InterfaceConnFailedException();
    }
    if ( activity.isValid() )
    {
        QAndroidJniObject::callStaticMethod<void>("com/example/android/tools/NotificationClient",
                                                  "notify",
                                                  "(Landroid/content/Context;Ljava/lang/String;)V",
                                                  QtAndroid::androidContext().object(),
                                                  QAndroidJniObject::fromString(m_notification).object<jstring>());
        if (_env->ExceptionCheck()) {
            _env->ExceptionClear();
            throw InterfaceConnFailedException();
        }
    }else
        throw InterfaceConnFailedException();


#endif

}
