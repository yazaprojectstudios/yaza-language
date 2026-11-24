plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "dev.yaza.mobile"
    compileSdk = 35

    defaultConfig {
        applicationId = "dev.yaza.mobile"
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    sourceSets.getByName("main").jniLibs.srcDir(layout.buildDirectory.dir("generated/yazaJniLibs"))
}

val buildYazaEngine by tasks.registering(Exec::class) {
    onlyIf { System.getenv("YAZA_SKIP_ENGINE_BUILD") != "1" }
    workingDir(rootProject.projectDir.parentFile.parentFile)
    val envYaza = System.getenv("YAZA_BIN")?.takeIf { File(it).canExecute() }
    val homeYaza = File(System.getProperty("user.home"), ".yaza/bin/yaza")
        .takeIf { it.canExecute() }?.absolutePath
    val yaza = envYaza ?: homeYaza ?: "yaza"
    val requestedTasks = gradle.startParameter.taskNames.map { it.substringAfterLast(':').lowercase() }
    val releaseRequested = requestedTasks.any { "release" in it }
    val debugRequested = requestedTasks.any { "debug" in it }
    val broadBuildRequested = requestedTasks.any { it in setOf("build", "assemble", "bundle") }
    val mode = if (debugRequested && !releaseRequested && !broadBuildRequested) "--debug" else "--release"
    commandLine(yaza, "mobile", "build", "--target", "android-arm64", mode)
}

tasks.named("preBuild").configure {
    dependsOn(buildYazaEngine)
}

