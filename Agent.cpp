#include "Agent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "BihTree.h"

using namespace std;

//const float HRVO_EPSILON = 0.00001f;
//const float HRVO_PI = 3.141592653589793f;




void Agent::computeNewVelocity(VODump* dump)
{
    vector<VelocityObstacle> velocityObstacles;
	velocityObstacles.reserve(m_neighbors.size());

	VelocityObstacle velocityObstacle;

	for (auto iter = m_neighbors.begin(); iter != m_neighbors.end(); ++iter)
    {
		const Object* otherObj = iter->second;
        const Circle* otherCirc = dynamic_cast<const Circle*>(otherObj);
        if (otherCirc != nullptr)
        {
            const Agent* otherAgent = dynamic_cast<const Agent*>(otherCirc);
		    if (absSq(otherCirc->m_position - m_position) > sqr(otherCirc->m_radius + m_radius)) // not intersecting
		    {
			    const float angle = atan(otherCirc->m_position - m_position);
			    const float openingAngle = std::asin((otherCirc->m_radius + m_radius) / abs(otherCirc->m_position - m_position));

			    velocityObstacle.m_side1 = Vec2(std::cos(angle - openingAngle), std::sin(angle - openingAngle));
			    velocityObstacle.m_side2 = Vec2(std::cos(angle + openingAngle), std::sin(angle + openingAngle));

			    const float d = 2.0f * std::sin(openingAngle) * std::cos(openingAngle);

                if (otherAgent != nullptr && otherAgent->m_isMobile) {
                    if (det(otherAgent->m_position - m_position, m_prefVelocity - otherAgent->m_prefVelocity) > 0.0f) {
                        const float s = 0.5f * det(m_velocity - otherAgent->m_velocity, velocityObstacle.m_side2) / d;

                        velocityObstacle.m_apex = otherAgent->m_velocity + s * velocityObstacle.m_side1; 
                    }
                    else {
                        const float s = 0.5f * det(m_velocity - otherAgent->m_velocity, velocityObstacle.m_side1) / d;

                        velocityObstacle.m_apex = otherAgent->m_velocity + s * velocityObstacle.m_side2;
                    }
                }
                else {
                    velocityObstacle.m_apex = Vec2(0.0f, 0.0f); // non moving neighbor the apes is 0
                }

		    }
		    else {
                if (otherAgent != nullptr && otherAgent->m_isMobile) {
				    velocityObstacle.m_apex = 0.5f * (otherAgent->m_velocity + m_velocity); 
                }
                else {
                    velocityObstacle.m_apex = Vec2(0.0f, 0.0f);
                }
			    velocityObstacle.m_side1 = normal(m_position, otherCirc->m_position);
			    velocityObstacle.m_side2 = -velocityObstacle.m_side1;
			    
		    }
        }
        else  // not a circle
        { 
            const Object* otherAab = dynamic_cast<const Object*>(otherObj);

            velocityObstacle.m_apex = Vec2(0.0f, 0.0f);

            Vec2 p1, p2;
            if (otherAab->spanningPoints(m_position, m_radius, &p1, &p2)) //m_radius
            {// outside
                velocityObstacle.m_side1 = normalize(p1 - m_position);
                velocityObstacle.m_side2 = normalize(p2 - m_position);
                if (det(velocityObstacle.m_side1, velocityObstacle.m_side2) < 0)
                    continue; // ignore
            }
            else {
            //    velocityObstacle.m_side1 = normal(m_position, otherAab->m_position);
            //    velocityObstacle.m_side2 = -velocityObstacle.m_side1;

                // alternate segment inside handling
                velocityObstacle.m_side1 = normalize(p1 - m_position);
                velocityObstacle.m_side2 = normalize(p2 - m_position);

            }

        }

        velocityObstacles.push_back(velocityObstacle);
	}

    Candidate candidate;

    Candidate minCandidate;
    float minScore = FLT_MAX;

    auto checkCandidate = [&]() {
        float score = absSq(m_prefVelocity - candidate.m_position);
        if (score >= minScore)
            return;

        for (int j = 0; j < (int)velocityObstacles.size(); ++j) 
        {
            if (j != candidate.m_velocityObstacle1 && j != candidate.m_velocityObstacle2 && 
                det(velocityObstacles[j].m_side2, candidate.m_position - velocityObstacles[j].m_apex) < 0.0f && 
                det(velocityObstacles[j].m_side1, candidate.m_position - velocityObstacles[j].m_apex) > 0.0f) 
            {
                return;
            }
        }
        minCandidate = candidate;
        minScore = score;
    };

    //std::multimap<float, Candidate> candidates;


	candidate.m_velocityObstacle1 = std::numeric_limits<int>::max();
	candidate.m_velocityObstacle2 = std::numeric_limits<int>::max();

	if (absSq(m_prefVelocity) < m_maxSpeed * m_maxSpeed) {
		candidate.m_position = m_prefVelocity;
	}
	else {
		candidate.m_position = m_maxSpeed * normalize(m_prefVelocity);
	}
    // first candidate, the preferred velocity capped to the max speed
    checkCandidate();
	//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));

#if 1
    // project perf velocity on sides of all vos
	for (int i = 0; i < (int)velocityObstacles.size(); ++i) 
    {
		candidate.m_velocityObstacle1 = i;
		candidate.m_velocityObstacle2 = i;

		const float dotProduct1 = (m_prefVelocity - velocityObstacles[i].m_apex) * velocityObstacles[i].m_side1;
		const float dotProduct2 = (m_prefVelocity - velocityObstacles[i].m_apex) * velocityObstacles[i].m_side2;

		if (dotProduct1 > 0.0f && det(velocityObstacles[i].m_side1, m_prefVelocity - velocityObstacles[i].m_apex) > 0.0f) 
        {
			candidate.m_position = velocityObstacles[i].m_apex + dotProduct1 * velocityObstacles[i].m_side1;

			if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}
		}

		if (dotProduct2 > 0.0f && det(velocityObstacles[i].m_side2, m_prefVelocity - velocityObstacles[i].m_apex) < 0.0f) 
        {
			candidate.m_position = velocityObstacles[i].m_apex + dotProduct2 * velocityObstacles[i].m_side2;

			if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}
		}
	}
#endif
#if 1
	for (int j = 0; j < (int)velocityObstacles.size(); ++j) 
    {
		candidate.m_velocityObstacle1 = std::numeric_limits<int>::max();
		candidate.m_velocityObstacle2 = j;

		float discriminant = m_maxSpeed * m_maxSpeed - sqr(det(velocityObstacles[j].m_apex, velocityObstacles[j].m_side1));

		if (discriminant > 0.0f)
        {
			const float t1 = -(velocityObstacles[j].m_apex * velocityObstacles[j].m_side1) + std::sqrt(discriminant);
			const float t2 = -(velocityObstacles[j].m_apex * velocityObstacles[j].m_side1) - std::sqrt(discriminant);

			if (t1 >= 0.0f) {
				candidate.m_position = velocityObstacles[j].m_apex + t1 * velocityObstacles[j].m_side1;
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}

			if (t2 >= 0.0f) {
				candidate.m_position = velocityObstacles[j].m_apex + t2 * velocityObstacles[j].m_side1;
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}
		}

		discriminant = m_maxSpeed * m_maxSpeed - sqr(det(velocityObstacles[j].m_apex, velocityObstacles[j].m_side2));

		if (discriminant > 0.0f)
        {
			const float t1 = -(velocityObstacles[j].m_apex * velocityObstacles[j].m_side2) + std::sqrt(discriminant);
			const float t2 = -(velocityObstacles[j].m_apex * velocityObstacles[j].m_side2) - std::sqrt(discriminant);

			if (t1 >= 0.0f) {
				candidate.m_position = velocityObstacles[j].m_apex + t1 * velocityObstacles[j].m_side2;
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}

			if (t2 >= 0.0f) {
				candidate.m_position = velocityObstacles[j].m_apex + t2 * velocityObstacles[j].m_side2;
				//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                checkCandidate();
			}
		}
	}
#endif
#if 1


	for (int i = 0; i < (int)velocityObstacles.size() - 1; ++i) 
    {
		for (int j = i + 1; j < static_cast<int>(velocityObstacles.size()); ++j) 
        {
			candidate.m_velocityObstacle1 = i;
			candidate.m_velocityObstacle2 = j;

			float d = det(velocityObstacles[i].m_side1, velocityObstacles[j].m_side1);

			if (d != 0.0f) {
				const float s = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[j].m_side1) / d;
				const float t = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[i].m_side1) / d;

				if (s >= 0.0f && t >= 0.0f) 
                {
					candidate.m_position = velocityObstacles[i].m_apex + s * velocityObstacles[i].m_side1;

					if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
						//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                        checkCandidate();
					}
				}
			}

			d = det(velocityObstacles[i].m_side2, velocityObstacles[j].m_side1);

			if (d != 0.0f) {
				const float s = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[j].m_side1) / d;
				const float t = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[i].m_side2) / d;

				if (s >= 0.0f && t >= 0.0f) {
					candidate.m_position = velocityObstacles[i].m_apex + s * velocityObstacles[i].m_side2;

					if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
						//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                        checkCandidate();
					}
				}
			}

			d = det(velocityObstacles[i].m_side1, velocityObstacles[j].m_side2);

			if (d != 0.0f) {
				const float s = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[j].m_side2) / d;
				const float t = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[i].m_side1) / d;

				if (s >= 0.0f && t >= 0.0f) {
					candidate.m_position = velocityObstacles[i].m_apex + s * velocityObstacles[i].m_side1;

					if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
						//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                        checkCandidate();
					}
				}
			}

			d = det(velocityObstacles[i].m_side2, velocityObstacles[j].m_side2);

			if (d != 0.0f) {
				const float s = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[j].m_side2) / d;
				const float t = det(velocityObstacles[j].m_apex - velocityObstacles[i].m_apex, velocityObstacles[i].m_side2) / d;

				if (s >= 0.0f && t >= 0.0f) {
					candidate.m_position = velocityObstacles[i].m_apex + s * velocityObstacles[i].m_side2;

					if (absSq(candidate.m_position) < m_maxSpeed * m_maxSpeed) {
						//candidates.insert(std::make_pair(absSq(m_prefVelocity - candidate.m_position), candidate));
                        checkCandidate();
					}
				}
			}
		}
	}
#endif

	//int optimal = -1;

    m_newVelocity = Vec2();
    if (minScore != FLT_MAX)
        m_newVelocity = minCandidate.m_position;
/*	for (auto iter = candidates.begin(); iter != candidates.end(); ++iter) 
    {
		candidate = iter->second;

		bool valid = true;

		for (int j = 0; j < (int)velocityObstacles.size(); ++j) 
        {
			if (j != candidate.m_velocityObstacle1 && j != candidate.m_velocityObstacle2 && 
                det(velocityObstacles[j].m_side2, candidate.m_position - velocityObstacles[j].m_apex) < 0.0f && 
                det(velocityObstacles[j].m_side1, candidate.m_position - velocityObstacles[j].m_apex) > 0.0f) 
            {
				valid = false;

				if (j > optimal) {
					optimal = j;
                    // I don't know WTF is this shit but it's not good.
    		        //m_newVelocity = candidate.m_position;
				}

				break;
			}
		}

		if (valid) {
    	    m_newVelocity = candidate.m_position;
			break;
		}
	}*/

    if (dump != nullptr) {
        dump->vos = velocityObstacles;
//        dump->candidates = candidates;
        dump->selected = m_newVelocity;
    }

}



void Agent::computePreferredVelocity(float deltaTime)
{
    //Vec2 toGoal = m_curGoalPos.p - m_position;
    Vec2 goalPnt = m_curGoalPos->getDest(m_position);
    Vec2 toGoal = goalPnt - m_position;

	const float distSqToGoal = absSq(toGoal);

	if (m_curGoalPos->shouldTaper() && sqr(m_prefSpeed * deltaTime) > distSqToGoal) 
    { // close to the goal? the point goal is the final one and we should not overshoot it
		m_prefVelocity = (toGoal) / deltaTime;
	}
	else 
    { // in segment goals we can overshoot the line, that's intended.
		m_prefVelocity = (m_prefSpeed / std::sqrt(distSqToGoal)) * toGoal;
	}
}

void Agent::computeNeighbors(BihTree& bihTree)
{
    m_neighbors.clear();

    float adjustingRangeSq = m_neighborDist * m_neighborDist;
    bihTree.query(m_position, m_neighborDist, [&](Object* obj) {
        insertNeighbor(obj, adjustingRangeSq);
    });
}


void Agent::insertNeighbor(Object* otherObj, float &rangeSq)
{
	if (this == otherObj) 
        return;


    // distance to where it touches me (to the point on the surface of the object closest to me), squared
    // there's no way to save this sqrt
    float checkSq = otherObj->distSqToSurface(m_position);


    if (checkSq < rangeSq) {
		if (m_neighbors.size() == m_maxNeighbors) {
			m_neighbors.erase(--m_neighbors.end());
		}

		m_neighbors.insert(std::make_pair(checkSq, otherObj));

		if (m_neighbors.size() == m_maxNeighbors) {
			rangeSq = (--m_neighbors.end())->first;
		}
	}
	
}

bool Agent::update(float deltaTime)
{
	const float dv = abs(m_newVelocity - m_velocity);

	if (dv < m_maxAccel * deltaTime) {
		m_velocity = m_newVelocity;
	}
	else {
		m_velocity = (1.0f - (m_maxAccel * deltaTime / dv)) * m_velocity + (m_maxAccel * deltaTime / dv) * m_newVelocity;
	}

	m_position += m_velocity * deltaTime;

    //bool reachedGoal = ( (absSq(m_curGoalPos.p - m_position) < m_goalRadius * m_goalRadius) );
    bool reachedGoal = m_curGoalPos->isPassed(m_position);
    bool reachedEnd = false;
    if (reachedGoal) {
        if (m_indexInPlan < m_plan.m_d.size()) {
            m_curGoalPos = m_plan.m_d[m_indexInPlan];
            ++m_indexInPlan;
        }
        else
            reachedEnd = true;
    }

	/*if (!reachedEnd) {
		m_orientation = atan(m_prefVelocity);
	}*/
    return reachedEnd;
}

