#include <JuceHeader.h>

//==============================================================================
// Runs every juce::UnitTest that the other files of this target register, and
// returns a failure to CTest if any of them fails.
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;

    return failures == 0 ? 0 : 1;
}
