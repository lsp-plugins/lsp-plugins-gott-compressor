<?php
	plugin_header();
	$m      =   (strpos($PAGE, '_mono') > 0) ? 'm' : (
				(strpos($PAGE, '_stereo') > 0) ? 's' : (
				(strpos($PAGE, '_lr') > 0) ? 'lr' : (
				(strpos($PAGE, '_ms') > 0) ? 'ms' : '?'
				)));
	$sc     =   (strpos($PAGE, 'sc_') === 0);
	$sm     =   ($m == 'ms') ? ' M, S' : (($m != 'm') ? ' L, R' : '');
?>

<p>
	This plugins introduces Grand Over-The-Top (GOTT) Compressor which combines both upward and
	downward compression and allows to smear the dynamics of the music - all what is often required
	in modern electronic music (and not only).
</p>
<p>
	The compressor provides three bands by default and allows to enable one more extra band at high
	frequencies.
</p>
<p>
	As opposite to alternatives, this compressor provides numerous special functions listed below:
</p>
<ul>
	<li><b>Modern operating mode</b> - special operating mode that allows to look different at <b>classic</b> crossover-based compressors.
	Crossover-based compressors use crossover filters for splitting the original signal into independent frequency bands, then process
	each band independently by it's individual compressor. Finally, all bands become phase-compensated using all-pass filers and then
	summarized, so the output signal is formed.
	In <b>Modern</b> mode, each band is processed by pair of dynamic shelving filters. This allows the better control the gain of each band.
	</li>
	<li><b>Sidechain boost</b> - special mode for assigning the same weight for higher frequencies opposite to lower frequencies.
	In usual case, the frequency band is processed by compressor 'as is'. By the other side, the usual audio signal has 3 db/octave
	falloff in the frequency domain and could be compared with the pink noise. So the lower frequencies take more
	effect on compressor rather than higher frequencies. <b>Sidechain boost</b> feature allows to compensate the -3 dB/octave falloff
	of the signal spectrum and, even more, make the signal spectrum growing +3 dB/octave in the almost fully audible frequency range.
	This is done by specially designed +3 db/oct and +6 db/oct shelving filters.
	</li>
	<li><b>Lookahead option</b> - each band of compressor can work with some prediction, the lookahead time can be set for each channel independently.
	To avoid phase distortions, all other bands automatically become delayed for a individually calculated period of time. The overall delay time
	of the input signal is reported to the host by the plugin as a latency.
	</li>
	<li><b>Surge protection</b> - provides surge protection mechanism aimed to avoid extra loud first pop when compressor starts working
	after a long period of silence.
	</li>
</ul>

<p><b>Controls:</b></p>
<ul>
	<li>
		<b>Bypass</b> - bypass switch, when turned on (led indicator is shining), the plugin bypasses signal.
	</li>
	<li><b>Mode</b> - combo box that allows to switch between <b>Modern</b> and <b>Classic</b> operating modes.</li>
	<li><b>SC Boost</b> - enables addidional boost of the sidechain signal:</li>
	<ul>
		<li><b>None</b> - no sidechain boost is applied.</li>
		<li><b>Pink BT</b> - a +3db/octave sidechain boost using bilinear-transformed shelving filter.</li>
		<li><b>Pink MT</b> - a +3db/octave sidechain boost using matched-transformed shelving filter.</li>
		<li><b>Brown BT</b> - a +6db/octave sidechain boost using bilinear-transformed shelving filter.</li>
		<li><b>Brown MT</b> - a +6db/octave sidechain boost using matched-transformed shelving filter.</li>
	</ul>
	<li><b>Zoom</b> - zoom fader, allows to adjust zoom on the frequency chart.</li>
</ul>
<p><b>'Analysis' section:</b></p>
<ul>
	<li><b>Reactivity</b> - the reactivity (smoothness) of the spectral analysis.</li>
	<li><b>Shift</b> - allows to adjust the overall gain of the analysis.</li>
	<li><b>FFT<?= $sm ?> In</b> - enables FFT curve graph of input signal on the spectrum graph.</li>
	<li><b>FFT<?= $sm ?> Out</b> - enables FFT curve graph of output signal on the spectrum graph.</li>
	<li><b>Filters</b> - enables drawing tranfer function of each sidechain filter on the spectrum graph.</li>
	<li><b>Surge</b> - enables surge protection mechanism.</li>
</ul>
<p><b>'Signal' section:</b></p>
<ul>
	<li><b>Input</b> - the amount of gain applied to the input signal before processing.</li>
	<li><b>Output</b> - the amount of gain applied to the output signal before processing.</li>
	<li><b>Dry</b> - the amount of dry (unprocessed) signal passed to the output.</li>
	<li><b>Wet</b> - the amount of wet (processed) signal passed to the output.</li>
	<li><b>In</b> - the input signal meter.</li>
	<li><b>Out</b> - the output signal meter.</li>
</ul>
<p><b>'Sidechain' section:</b></p>
<ul>
	<li><b>Setup</b> - combo boxes that allow to control sidechain working mode:</li>
	<ul>
		<li><b>Peak</b> - peak mode.</li>
		<li><b>RMS</b> - Root Mean Square (SMA) of the input signal.</li>
		<li><b>LPF</b> - input signal processed by recursive 1-pole Low-Pass Filter (LPF).</li>
		<li><b>SMA</b> - input signal processed by Simple Moving Average (SMA) filter.</li>
		<?php if ($m != 'm') { ?>
			<li><b>Middle</b> - middle part of signal is used for sidechain processing.</li>
			<li><b>Side</b> - side part of signal is used for sidechain processing.</li>
			<li><b>Left</b> - only left channel is used for sidechain processing.</li>
			<li><b>Right</b> - only right channel is used for sidechain processing.</li>
			<li><b>Min</b> - the absolute minimum value is taken from stereo input.</li>
			<li><b>Max</b> - the absolute maximum value is taken from stereo input.</li>
		<?php } ?>
	</ul>
	<li><b>Preamp</b> - pre-amplification of the sidechain signal.</li>
	<li><b>Reactivity</b> - reactivity of the sidechain signal.</li>
	<li><b>Lookahead</b> - look-ahead time of the sidechain relative to the input signal.</li>
</ul>
<p><b>'Compressor' section:</b></p>
<ul>
	<li><b>LOW, MID, HIGH, HI_E</b> - enables compressor for corresponding band.</li>
	<li><b>S</b> - enables soloing mode for corresponding band.</li>
	<li><b>M</b> - mutes corresponding band.</li>
	<li><b>ON</b> - enables extra band.</li>
	<li><b>Frequency</b> - the knob that allows to control the split frequency between bands.</li>
	<li><b>Bottom</b> - the bottom threshold, below which any upward compression is disabled.</li>
	<li><b>Upward theshold</b> - the threshold, below which the upward compression starts working.</li>
	<li><b>Upward ratio</b> - the ratio of the upward compression.</li>
	<li><b>Downward theshold</b> - the threshold, above which the downward compression starts working.</li>
	<li><b>Downward ratio</b> - the ratio of the downward compression.</li>
	<li><b>Attack</b> - the compressor's attack time.</li>
	<li><b>Release</b> - the compressor's release time.</li>
	<li><b>Makeup</b> - the makeup gain for the corresponding band.</li>
	<li><b>Knee</b> - the compressor's knee.</li>
	<li><b>Compression curve</b> - the compression curve graph and the gain reduction meter.</li>
</ul>



