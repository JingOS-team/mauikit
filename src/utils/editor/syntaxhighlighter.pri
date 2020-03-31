QT *= core qml quick gui

android {
    exists($$PWD/KSyntaxHighlighting) {
        message("Using KSyntaxHighlighting for Android")

    }else {
        warning("Getting KSyntaxHighlighting for Android")
        system(git clone $$KSYNTAXHIGHLIGHTING_REPO $$PWD/KSyntaxHighlighting)
    }

    ANDROID_EXTRA_LIBS += $$PWD/KSyntaxHighlighting/libKF5SyntaxHighlighting.so

    LIBS += -L$$PWD/KSyntaxHighlighting/ -lKF5SyntaxHighlighting

    INCLUDEPATH += $$PWD/KSyntaxHighlighting/KSyntaxHighlighting

    DEPENDPATH += $$PWD/KSyntaxHighlighting/KSyntaxHighlighting
}else:win32 {

}else:macos {

    LIBS += -L$$PWD/../../../../../../../../../usr/local/Cellar/kf5-syntax-highlighting/5.68.0/lib/ -lKF5SyntaxHighlighting.5.68.0

    INCLUDEPATH += $$PWD/../../../../../../../../../usr/local/Cellar/kf5-syntax-highlighting/5.68.0/include
    DEPENDPATH += $$PWD/../../../../../../../../../usr/local/Cellar/kf5-syntax-highlighting/5.68.0/include

}

