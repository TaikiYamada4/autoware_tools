# regulatory_element_details_for_traffic_lights

## Validator name

mapping.traffic_light.correct_facing

## Feature

This validator checks whether each `traffic_light` linestring is drawn with the correct direction, becuase the linestring direction describes the facing of the traffic_light.
If the traffic light is facing to the viewer, the traffic light linestring must be drawn from the left point to the right point seen from the viewer.
Note that this validator only check traffic lights whose subtype are `red_yellow_green`.
This validator checks five types of issues.
The former three issues are related to prerequisites to perform correct validation rather than direct validation results of the traffic light facing.
The latter two issues mention to the traffic light facing.

All output issues specify the traffic_light "linestring" or the traffic_light "regulatory_element" as the **primitive**, and the primitive ID will be specified as the **ID**.

| Message                                                                        | Severity | Primitive          | Description                                                                                                                                                                                                                  |
| ------------------------------------------------------------------------------ | -------- | ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| "Lanelets referring this traffic_light have several divergent starting lines"  | Warning  | Linestring         | A `traffic_light` subtype regulatory element may be referred by multiple lanelets. This warning appears when the starting line of those lanelets (which tends to be the same or similar) diverge too much.                   |
| "Regulatory element of traffic light must be referred by at least one lanelet" | Warning  | Regulatory element | There is a `traffic_light` subtype regulatory element that haven't be referred by one or more lanelets. In other words, there are no `lanelet`s that has this regulatory element as a `relation` member.                     |
| "This traffic light hasn't been referred to any regulatory element."           | Error    | Linestring         | Traffic lights must have a regulatory element that refers to them while this traffic light doesn't have one.                                                                                                                 |
| "The linestring direction seems to be wrong."                                  | Error    | Linestring         | This `traffic_light` type linestring is drawn with the wrong direction.                                                                                                                                                      |
| "The linestring direction has been judged as both correct and wrong."          | Warning  | Linestring         | The validator cannot judge whether the direction of this `traffic_light` type linestring is correct. (Mostly they are correct.) This occurs from special regulatory element definitions and techinical issues written below. |

### Procedure

This flow chart shows the simplized procedure how the validation is done.

![traffic_light_facing_procedure](../../media/traffic_light_facing_procedure.svg)

### Difficult cases

Currently, this validator assumes that all traffic lights in the same regulatory element has the same facing.
However, there might be cases that this assumption doesn't hold, and this is only when traffic lights on the other side of the road is in the same regulatory element.
It is hard to tell that this traffic light is for this road or the opposite road since the facing of the traffic light is unknown yet, so this validator keeps its ambiguity for now.
This kind of traffic light will be judged as correct from the this side but not from the other side, and it will be misjudjed oppositely if the traffic light linestring id drawn wrong.
This validator will throws a warning for this case and tells the user to check it by their own.

We assume that this kind of traffic light could be found only a few, but if you feel this concerning you can remove the traffic light on the other road from the regulatory element. This workaround affects nothing if your planning module doesn't utilize the information that the traffic light on the other road has the same timing of lighting.

![difficult_cases_in_traffic_light_facing](../../media/traffic_light_facing_difficult_cases.svg)

## Related source codes

- traffic_light_facing.hpp
- traffic_light_facing.cpp
