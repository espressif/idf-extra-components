# CI in idf-extra-components

## Build and test apps

The workflow defined in [build_and_run_apps.yml](workflows/build_and_run_apps.yml) builds the apps (examples, test apps) and runs the tests on self-hosted runners.


```mermaid
flowchart TD
        PR((Pull Request))
        PR -->labels

        schedule((Schedule<br>Push to master))
        schedule -->idf-build-apps-build

    subgraph "Generate pipeline"
        labels[Get labels] --> get-changes
        get-changes[Get changed files]
        get-changes --> build-all
        build-all{Build all apps<br> label set?}
        build-all --> |yes| changed-components
        changed-components --> idf-build-apps-args
        build-all --> |no| idf-build-apps-args
        changed-components[Get changed components]
        idf-build-apps-args[Prepare idf-build-apps arguments]
    end
    subgraph "Build apps"
        idf-build-apps-args --> idf-build-apps-build
        idf-build-apps-build[idf-build-apps build] -->
        build-only
        build-only{Build only<br>label set?}
        build-only --> |no| upload-artifacts

        upload-artifacts[Upload artifacts]

    end
    subgraph "Test apps"
        upload-artifacts -->download-artifacts
        download-artifacts[Download artifacts] -->pytest
        pytest[Pytest] -->upload-results
        upload-results[Upload results]
    end
    subgraph "Generate report"
        upload-results -->download-results
        download-results[Download results] -->generate-report
        generate-report[Generate report]
    end

    build-only --> |yes| fin
    generate-report -->fin
    fin([Finish])
```
