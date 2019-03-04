#include "hep/mc.hpp"

#include <iostream>

double square(hep::mc_point<double> const& x)
{
    return 3.0 * x.point()[0] * x.point()[0];
}

int main()
{
    // integrate the square function with two iterations and five bins
    auto results = hep::vegas(
        hep::make_integrand<double>(square, 1),
        std::vector<std::size_t>(2, 100),
        hep::make_vegas_chkpt<double>(5)
    ).results();

    // write the pdf that was used to generate the last iteration
    results.back().pdf().serialize(std::cout);
    std::cout << '\n';

    return 0;
}

