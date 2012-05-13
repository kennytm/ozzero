functor
import
    OS

export
    seed: Seed
    uniformBetween: UniformBetween
    uniform: Uniform

define
    % Seed the random number generator
    proc {Seed}
	{OS.srand 0}
    end

    % Convenient function to retrieve a (roughly) uniformly random integer.
    fun {UniformBetween MinI MaxI}
	RandMinI RandMaxI
	RandInterval
	Interval = MaxI - MinI + 1
    in
        {OS.randLimits RandMinI RandMaxI}
	RandInterval = RandMaxI - RandMinI + 1
        (({OS.rand} - RandMinI) * Interval) div RandInterval + MinI
    end

    fun {Uniform LimitI}
	{UniformBetween 0 LimitI-1}
    end
end

