# ESP-IDF Legacy Unit-Test-App

Older version of ESP-IDF, all Unity tests were built using a common app, this project's unit-test-app. ESP-IDF has since migrated to a more modular approach, where each component has its own set of test apps. These test apps are built for the specified sdkconfigs using [idf-build-apps](https://github.com/espressif/idf-build-apps) and running the tests are automated with [pytest-embedded](https://github.com/espressif/pytest-embedded).   

This component has the unit-test-app as an example, to allow for easy integration by creating a project from it with:

`idf.py create-project-from-example "espressif/unit-test-app:unit-test-app`

For instructions on how to use the unit-test-app please see the [example README.md](examples/unit-test-app/README.md).
